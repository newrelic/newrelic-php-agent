//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package infinite_tracing

import (
	"errors"
	"sync"
	"time"

	"newrelic/log"
)

const (
	// recordSpanBackoff is the time to wait after a failure on the RecordSpan
	// endpoint before retrying
	recordSpanBackoff = 15 * time.Second
	// numCodes is the total number of grpc.Codes
	numCodes = 17
	// supportability metrics
	supportabilitySent        = "Supportability/InfiniteTracing/Span/Sent"
	supportabilityResponseErr = "Supportability/InfiniteTracing/Span/Response/Error"
	supportabilityQueueDumped = "Supportability/InfiniteTracing/Span/AgentQueueDumped"
)

type spanBatch struct {
	count uint64
	batch []byte
}

type encodedSpanBatch []byte

type spanBatchSenderCode int
type spanBatchSenderStatus struct {
	code   spanBatchSenderCode
	metric string
}

const (
	statusOk spanBatchSenderCode = iota
	// An unrecoverable error occured, the sender has to be shut down.
	statusShutdown
	// An error occured, another connection attempt should be made after the
	// backoff wait time.
	statusRestart
	// An error occured, another TCP connection attempt should be made,
	// resolving the trace observer host name anew (to account for cell
	// shifts).
	statusReconnect
	// An error occured, another connection attempt should be made
	// immediately.
	statusImmediateRestart
)

// The sender is split out via an interface. This is not strictly necessary,
// but makes it easier to test things.
type spanBatchSender interface {
	// This method has to be called successfully before the send method can
	// be called. In case of connection problems this method can be called
	// again.
	connect() (error, spanBatchSenderStatus)
	send(batch encodedSpanBatch) (error, spanBatchSenderStatus)
	// Errors reported asynchronously are reported via the responseError
	// channel.
	response() chan spanBatchSenderStatus
	shutdown()
	clone() (spanBatchSender, error)
}

type TraceObserver struct {
	// The span batch queue.
	messages                  chan *spanBatch
	closeMessagesOnce         sync.Once
	messagesSent              chan uint64
	messagesRemainingCapacity uint64

	// Channels to send shutdown messages.
	initiateShutdown        chan struct{}
	initiateShutdownOnce    sync.Once
	initiateAppShutdown     chan struct{}
	initiateAppShutdownOnce sync.Once
	shutdownComplete        chan struct{}

	// A channel that holds errors returned from gRPC.
	responseError chan spanBatchSenderStatus

	// Trace observer supportability metrics.
	supportability *traceObserverSupportability

	sender spanBatchSender

	Config
}

type Config struct {
	Host              string
	Port              uint16
	Secure            bool
	License           string
	RunId             string
	QueueSize         uint64
	RequestHeadersMap map[string]string
}

type metricIncrement struct {
	name  string
	count float64
}

type traceObserverSupportability struct {
	incrementSent chan float64
	increment     chan metricIncrement
	dump          chan map[string]float64
}

func newTraceObserverWithWorker(cfg *Config) (*TraceObserver, func()) {
	to := &TraceObserver{
		messages:                  make(chan *spanBatch, cfg.QueueSize),
		messagesSent:              make(chan uint64, cfg.QueueSize),
		messagesRemainingCapacity: cfg.QueueSize,
		initiateShutdown:          make(chan struct{}),
		initiateAppShutdown:       make(chan struct{}),
		shutdownComplete:          make(chan struct{}),
		Config:                    *cfg,
		supportability:            newTraceObserverSupportability(),
	}
	go to.handleSupportability()
	worker := func() {
		for {
			to.responseError = to.sender.response()

			status := to.doStreaming()

			if status.code == statusShutdown {
				to.initShutdown()
				break
			} else if status.code == statusRestart {
				time.Sleep(recordSpanBackoff)
			} else if status.code == statusReconnect {
				to.sender.shutdown()

				sender, err := to.sender.clone()
				if err != nil {
					log.Debugf("cannot clone sender: %v", err)
					break
				}
				to.sender = sender

				log.Debugf("sender cloned for reconnect attempt")
			}
		}

		to.completeShutdown()
		to.sender.shutdown()
	}

	return to, worker
}

// Initialize a connection to a trace observer. This function returns
// immediately, while trying to establish a connection in the background.
//
// In case no connection can be established, span batches are dropped based on
// the backpressure handling.
func NewTraceObserver(cfg *Config) *TraceObserver {
	to, worker := newTraceObserverWithWorker(cfg)
	go func() {
		sender, err := newGrpcSpanBatchSender(cfg)
		if err != nil {
			return
		}

		to.sender = sender
		worker()
	}()

	return to
}

// Add a span batch to the queue.
//
// This should only be called on a single go routine. Concurrent calls might
// case race conditions due to missing locking around the capacity counter.
func (to *TraceObserver) QueueBatch(count uint64, batch []byte) {
	if to.isShutdownInitiated() {
		if !to.isShutdownComplete() {
			to.closeMessages()
		}
		return
	}

	// If the queue is full, flush the whole queue. This increases the
	// likelihood of sending complete traces.
	if to.getRemainingQueueCapacity() < count {
		to.emptyQueue()
	}

	b := &spanBatch{
		count: count,
		batch: batch,
	}

	to.messages <- b
	to.messagesRemainingCapacity -= count
}

// Shut down the trace observer connection. This blocks until the shutdown is
// complete or until the timeout is hit.
func (to *TraceObserver) Shutdown(timeout time.Duration) error {
	to.initShutdown()
	ticker := time.NewTicker(timeout)
	defer ticker.Stop()

	var err error
	select {
	case <-to.shutdownComplete:
		err = nil
	case <-ticker.C:
		err = errors.New("timeout exceeded while waiting for trace observer shutdown to complete")
	}
	to.closeMessages()

	return err
}

func (to *TraceObserver) emptyQueue() {
	var dropped uint64
	for {
		select {
		case batch := <-to.messages:
			dropped += batch.count
		default:
			to.messagesRemainingCapacity += dropped

			to.supportability.increment <- metricIncrement{
				name:  supportabilityQueueDumped,
				count: float64(dropped),
			}

			log.Debugf("trace observer dropped %d spans due to backpressure", dropped)
			return
		}
	}
}

func (to *TraceObserver) getRemainingQueueCapacity() uint64 {
	for {
		select {
		case sent := <-to.messagesSent:
			to.messagesRemainingCapacity += sent
		default:
			return to.messagesRemainingCapacity
		}
	}
}

func (to *TraceObserver) initShutdown() {
	to.initiateShutdownOnce.Do(func() {
		log.Debugf("initiating trace observer shutdown")
		close(to.initiateShutdown)
	})
}

func (to *TraceObserver) closeInitiateAppShutdown() {
	to.initiateAppShutdownOnce.Do(func() {
		close(to.initiateAppShutdown)
	})
}

func (to *TraceObserver) closeMessages() {
	to.closeMessagesOnce.Do(func() {
		close(to.messages)

		for range to.messages {
			// drain the channel
		}
	})
}

func (to *TraceObserver) completeShutdown() {
	if to.isShutdownComplete() {
		return
	}

	close(to.shutdownComplete)
	log.Debugf("trace observer shutdown completed")
}

func (to *TraceObserver) doStreaming() spanBatchSenderStatus {
	if err, status := to.sender.connect(); err != nil {
		to.supportabilityError(status)
		log.Errorf("cannot establish stream to trace observer endpoint: %v", err)
		return status
	}

	log.Debugf("established stream to trace observer endpoint")
	for {
		select {
		case msg := <-to.messages:
			log.Debugf("trace observer sending span batch of size %d, %d of %d remaining in queue",
				msg.count, to.messagesRemainingCapacity, to.QueueSize)
			if err, status := to.sender.send(encodedSpanBatch(msg.batch)); err != nil {
				to.messagesSent <- msg.count
				to.supportabilityError(status)
				log.Errorf("trace observer error while sending span batch:  %v", err)
				return status
			} else {
				to.messagesSent <- msg.count
				to.supportability.incrementSent <- float64(msg.count)
			}
		case status := <-to.responseError:
			to.supportabilityError(status)
			log.Debugf("trace observer error response received: %v", status)
			return status
		case <-to.initiateShutdown:
			log.Debugf("trace observer sending pending span batches due to shutdown")
			return spanBatchSenderStatus{code: statusShutdown}
		}
	}
}

func (to *TraceObserver) isShutdownComplete() bool {
	select {
	case <-to.shutdownComplete:
		return true
	default:
	}
	return false
}

func (to *TraceObserver) isShutdownInitiated() bool {
	select {
	case <-to.initiateShutdown:
		return true
	default:
	}
	return false
}

func (to *TraceObserver) isAppShutdownInitiated() bool {
	select {
	case <-to.initiateAppShutdown:
		return true
	default:
	}
	return false
}

func (to *TraceObserver) handleSupportability() {
	metrics := newSupportabilityMetrics()
	for {
		select {
		case <-to.initiateAppShutdown:
			// Close the goroutine once a shutdown was triggered
			// from the application. The goroutine should stay
			// alive when a shutdown was triggered from the sender.
			return
		case inc := <-to.supportability.increment:
			metrics[inc.name] += inc.count
		case batchCount := <-to.supportability.incrementSent:
			// Since we're sending span batches, we increment by the number of spans in the batch.
			metrics[supportabilitySent] += batchCount
		case to.supportability.dump <- metrics:
			// reset the metrics map
			metrics = newSupportabilityMetrics()
		}
	}
}

func newSupportabilityMetrics() map[string]float64 {
	// grpc codes, plus 1 for sent, and 1 for response errs
	metrics := make(map[string]float64, numCodes+2)
	// supportabilitySent metric must always be sent
	metrics[supportabilitySent] = 0
	return metrics
}

func newTraceObserverSupportability() *traceObserverSupportability {
	return &traceObserverSupportability{
		incrementSent: make(chan float64),
		increment:     make(chan metricIncrement),
		dump:          make(chan map[string]float64),
	}
}

// dumpSupportabilityMetrics reads the current supportability metrics off of
// the channel and resets them to 0.
func (to *TraceObserver) DumpSupportabilityMetrics() map[string]float64 {
	if to.isAppShutdownInitiated() {
		return nil
	}
	return <-to.supportability.dump
}

func (to *TraceObserver) supportabilityError(status spanBatchSenderStatus) {
	if status.metric != "" {
		to.supportability.increment <- metricIncrement{
			name:  status.metric,
			count: 1,
		}
		to.supportability.increment <- metricIncrement{
			name:  supportabilityResponseErr,
			count: 1,
		}
	}
}
