//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package infinite_tracing

import (
	"errors"
	"reflect"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"

	v1 "github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1"
)

type mockSpanBatchSender struct {
	cbConnect     func() (error, spanBatchSenderStatus)
	cbSend        func(batch encodedSpanBatch) (error, spanBatchSenderStatus)
	responseError chan spanBatchSenderStatus
	cloneAttempts uint
}

func (ms *mockSpanBatchSender) connect() (error, spanBatchSenderStatus) {
	return ms.cbConnect()
}

func (ms *mockSpanBatchSender) send(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
	return ms.cbSend(batch)
}

func (ms *mockSpanBatchSender) response() chan spanBatchSenderStatus {
	return ms.responseError
}

func (ms *mockSpanBatchSender) shutdown() {}

func (ms *mockSpanBatchSender) clone() (spanBatchSender, error) {
	ms.cloneAttempts += 1
	return ms, nil
}

func expectSupportabilityMetrics(t *testing.T, to *TraceObserver, expected map[string][6]float64) {
	t.Helper()
	actual := to.DumpSupportabilityMetrics()
	if !reflect.DeepEqual(expected, actual) {
		t.Errorf("Supportability metrics do not match.\nExpected: %#v\nActual: %#v\n", expected, actual)
	}
}

func TestConnectShutdownTrigger(t *testing.T) {
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			return errors.New(""), spanBatchSenderStatus{
				code:   statusShutdown,
				metric: "Supportability/InfiniteTracing/Span/gRPC/UNIMPLEMENTED",
			}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	to.sender = sender
	worker()

	if to.isShutdownComplete() != true {
		t.Errorf("unrecoverable connect error doesn't trigger a shutdown")
	}

	expectSupportabilityMetrics(t, to, map[string][6]float64{
		"Supportability/InfiniteTracing/Span/gRPC/UNIMPLEMENTED": [6]float64{1, 0, 0, 0, 0, 0},
		"Supportability/InfiniteTracing/Span/Response/Error":     [6]float64{1, 0, 0, 0, 0, 0},
		"Supportability/InfiniteTracing/Span/Sent":               [6]float64{0, 0, 0, 0, 0, 0},
	})

	// check that messages don't block the queue after shutdown
	var i uint64
	for i = 0; i < (to.QueueSize + 2); i++ {
		to.QueueBatch(1, []byte{1, 2, 3})
	}
}

func TestSendShutdownTrigger(t *testing.T) {
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			return errors.New(""), spanBatchSenderStatus{
				code:   statusShutdown,
				metric: "Supportability/InfiniteTracing/Span/gRPC/UNIMPLEMENTED",
			}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	go func() {
		to.sender = sender
		worker()
	}()

	if to.isShutdownComplete() != false {
		t.Errorf("successful connect not registered")
	}

	// check that messages don't block the queue after shutdown
	to.QueueBatch(1, []byte{1, 2, 3})

	// Wait for a timeout, or until the shutdown is complete
	ticker := time.NewTicker(500 * time.Millisecond)
	defer ticker.Stop()

Loop:
	for {
		if to.isShutdownComplete() == true {
			break
		}
		select {
		case <-ticker.C:
			break Loop
		default:
		}
	}

	expectSupportabilityMetrics(t, to, map[string][6]float64{
		"Supportability/InfiniteTracing/Span/gRPC/UNIMPLEMENTED": [6]float64{1, 0, 0, 0, 0, 0},
		"Supportability/InfiniteTracing/Span/Response/Error":     [6]float64{1, 0, 0, 0, 0, 0},
		"Supportability/InfiniteTracing/Span/Sent":               [6]float64{0, 0, 0, 0, 0, 0},
		"Supportability/PHP/InfiniteTracing/Output/Bytes":        [6]float64{1, 0, 0, 0, 0, 0},
	})

	if to.isShutdownComplete() != true {
		t.Errorf("unrecoverable send error doesn't trigger a shutdown")
	}
}

func TestShutdown(t *testing.T) {
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	go func() {
		to.sender = sender
		worker()
	}()

	if to.isShutdownComplete() != false {
		t.Errorf("successful connect not registered")
	}

	to.Shutdown(10 * time.Millisecond)

	if to.isShutdownComplete() != true {
		t.Errorf("call to Shutdown doesn't trigger a shutdown")
	}

	// repeated calls to Shutdown don't panic
	to.Shutdown(10 * time.Millisecond)
	to.Shutdown(10 * time.Millisecond)
}

func TestSend(t *testing.T) {
	received := make(chan []byte, 10)
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			received <- batch
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
	}
	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	defer to.Shutdown(10 * time.Millisecond)
	go func() {
		to.sender = sender
		worker()
	}()
	to.QueueBatch(1, []byte{1, 2, 3})
	to.QueueBatch(1, []byte{4, 5, 6})
	to.QueueBatch(1, []byte{7, 8, 9})
	batch := <-received
	if !reflect.DeepEqual(batch, []byte{1, 2, 3}) {
		t.Errorf("didn't receive span batch")
	}
	batch = <-received
	if !reflect.DeepEqual(batch, []byte{4, 5, 6}) {
		t.Errorf("didn't receive span batch")
	}
	batch = <-received
	if !reflect.DeepEqual(batch, []byte{7, 8, 9}) {
		t.Errorf("didn't receive span batch")
	}
}

// This test is deactivated for now, as it causes Jenkins test builds to
// randomly fail.
//
//func TestBackpressure(t *testing.T) {
//	blockSend := make(chan bool)
//
//	sender := &mockSpanBatchSender{
//		cbConnect: func() (error, spanBatchSenderStatus) {
//			return nil, spanBatchSenderStatus{code: statusOk}
//		},
//		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
//			blockSend <- true
//			return nil, spanBatchSenderStatus{code: statusOk}
//		},
//	}
//
//	to, worker := newTraceObserverWithWorker(&Config{
//		QueueSize: 10,
//	})
//	defer to.Shutdown(100 * time.Millisecond)
//	go func() {
//		to.sender = sender
//		worker()
//	}()
//
//	// 5 batches of size two should be queued, the queue is full.
//	for i := 0; i < 5; i++ {
//		to.QueueBatch(2, []byte{1, 2, 3})
//	}
//
//	// The next batch should trigger emptying the queue
//	to.QueueBatch(2, []byte{1, 2, 3})
//
//	// The next 3 batches should be sent
//	to.QueueBatch(2, []byte{1, 2, 3})
//	to.QueueBatch(2, []byte{1, 2, 3})
//	to.QueueBatch(2, []byte{1, 2, 3})
//
//	// Unblock the queue to send 4 batches
//	<-blockSend
//	<-blockSend
//	<-blockSend
//	<-blockSend
//
//	expectSupportabilityMetrics(t, to, map[string]float64{
//		"Supportability/InfiniteTracing/Span/Sent":             8,
//		"Supportability/InfiniteTracing/Span/AgentQueueDumped": 10,
//	})
//}

func TestSentSpanMetrics(t *testing.T) {
	sent := make(chan []byte, 10)
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			sent <- batch
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	defer to.Shutdown(10 * time.Millisecond)
	go func() {
		to.sender = sender
		worker()
	}()

	expectSupportabilityMetrics(t, to, map[string][6]float64{
		"Supportability/InfiniteTracing/Span/Sent": [6]float64{0, 0, 0, 0, 0, 0},
	})
	to.QueueBatch(1, []byte{1, 2, 3})
	to.QueueBatch(3, []byte{4, 5, 6})
	to.QueueBatch(1, []byte{7, 8, 9})

	<-sent
	<-sent
	<-sent

	time.Sleep(10 * time.Millisecond)

	expectSupportabilityMetrics(t, to, map[string][6]float64{
		"Supportability/InfiniteTracing/Span/Sent":        [6]float64{5, 0, 0, 0, 0, 0},
		"Supportability/PHP/InfiniteTracing/Output/Bytes": [6]float64{3, 9, 0, 0, 0, 0},
	})

	// Ensure counts are reset
	expectSupportabilityMetrics(t, to, map[string][6]float64{
		"Supportability/InfiniteTracing/Span/Sent": [6]float64{0, 0, 0, 0, 0, 0},
	})
}

func TestImmediateRestart(t *testing.T) {
	connectReturn := make(chan spanBatchSenderStatus, 10)
	received := make(chan encodedSpanBatch)
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			status := <-connectReturn
			if status.code != statusOk {
				return errors.New(""), status
			}
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			received <- batch
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	defer to.Shutdown(10 * time.Millisecond)
	go func() {
		to.sender = sender
		worker()
	}()

	// Force an immediate restart
	connectReturn <- spanBatchSenderStatus{code: statusImmediateRestart}
	connectReturn <- spanBatchSenderStatus{code: statusImmediateRestart}
	connectReturn <- spanBatchSenderStatus{code: statusOk}

	to.QueueBatch(1, []byte{1, 2, 3})

	// Wait for a timeout, or until the message was sent
	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	select {
	case <-received:
	case <-ticker.C:
		t.Errorf("didn't receive span batch, no immediate restart")
	}
}

func TestReconnect(t *testing.T) {
	connectReturn := make(chan spanBatchSenderStatus, 10)
	received := make(chan encodedSpanBatch)
	sender := &mockSpanBatchSender{
		cbConnect: func() (error, spanBatchSenderStatus) {
			status := <-connectReturn
			if status.code != statusOk {
				return errors.New(""), status
			}
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		cbSend: func(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
			received <- batch
			return nil, spanBatchSenderStatus{code: statusOk}
		},
		responseError: make(chan spanBatchSenderStatus, 10),
		cloneAttempts: 0,
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	defer to.Shutdown(10 * time.Millisecond)
	go func() {
		to.sender = sender
		worker()
	}()

	// Force a reconnect
	connectReturn <- spanBatchSenderStatus{code: statusReconnect}
	connectReturn <- spanBatchSenderStatus{code: statusReconnect}
	connectReturn <- spanBatchSenderStatus{code: statusOk}

	to.QueueBatch(1, []byte{1, 2, 3})

	// Wait for a timeout, or until the message was sent
	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	select {
	case <-received:
	case <-ticker.C:
		t.Errorf("didn't receive span batch")
	}

	if sender.cloneAttempts != 2 {
		t.Errorf("expected 2 clone attempts, got %v", sender.cloneAttempts)
	}
}

func TestOkCloseReconnectsWithoutBackoff(t *testing.T) {
	srv := newTestObsServer(t)
	srv.closeAfterOneMessage = true
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}

	to, worker := newTraceObserverWithWorker(&Config{
		QueueSize: 100,
	})
	defer to.Shutdown(10 * time.Millisecond)
	go func() {
		to.sender = sender
		worker()
	}()

	batch1, _ := proto.Marshal(&v1.SpanBatch{Spans: []*v1.Span{{TraceId: "first"}}})
	to.QueueBatch(1, batch1)

	select {
	case <-srv.spansReceivedChan:
	case <-time.After(1 * time.Second):
		t.Fatalf("didn't receive first span batch")
	}

	// srv.spansReceivedChan fires the instant the server's handler receives
	// the first message - before that handler returns and the OK close
	// trailer actually reaches the client over the socket. Without this
	// pause, queuing batch2 races the close notification: if it loses,
	// SendMsg can succeed locally on the already-server-closed stream
	// (the message is silently dropped, no error at all - the same
	// transport quirk TestSendAfterServerOkCloseGetsCorrectStatus polls
	// around), and the batch is gone before send() ever gets a chance to
	// classify anything. This isn't a fixed EOF-detection latency either -
	// it's comfortably longer than a loopback close round-trip should ever
	// take, so the pause resolves before it's noticed in the common case.
	time.Sleep(50 * time.Millisecond)

	// The server closes the stream with OK after that first message. A
	// second batch should still land quickly - well under the 15s
	// recordSpanBackoff - if the OK close is handled as an immediate
	// reconnect instead of a generic error.
	batch2, _ := proto.Marshal(&v1.SpanBatch{Spans: []*v1.Span{{TraceId: "second"}}})
	to.QueueBatch(1, batch2)

	select {
	case <-srv.spansReceivedChan:
	case <-time.After(2 * time.Second):
		t.Fatalf("second batch didn't arrive within 2s - looks like it hit the 15s backoff")
	}
}
