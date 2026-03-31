//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"sync"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/infinite_tracing"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

// This type takes the HarvestType values sent from an application's harvest
// trigger function, decorates them with the application, run ID, and harvest,
// and then sends them to a processor as ProcessorHarvest messages.  Whenever
// an AppHarvest is closed, an event is sent via the cancel channel so that
// the harvest trigger function may also be closed.
type AppHarvest struct {
	*App
	*Harvest
	*infinite_tracing.TraceObserver

	trigger          chan HarvestType
	cancel           chan bool
	MetricController *MetricsController
}

type metricsInfo struct {
	eventType string
	seen      float64
	sent      float64
	failed    float64
}

type dataUsageInfo struct {
	endpoint_name string
	payloadSize   int
	responseSize  int
}

type MetricsController struct {
	mc  chan metricsInfo
	duc chan dataUsageInfo
	wg  *sync.WaitGroup
	mu  sync.Mutex
}

func (ah *AppHarvest) NewProcessorHarvestEvent(id AgentRunID, t HarvestType) ProcessorHarvest {
	return ProcessorHarvest{
		AppHarvest: ah,
		ID:         id,
		Type:       t,
	}
}

func (m *MetricsController) AddMetricData(event string, seen, sent, failed float64) {
	select {
	case m.mc <- metricsInfo{
		eventType: event,
		seen:      seen,
		sent:      sent,
		failed:    failed,
	}:
		// data stored in channel
	default:
		// channel full
		log.Debugf("Metric Data Channel full, dropping data")

	}
}

func (m *MetricsController) addDataUsage(endpoint string, data_stored int, data_received int) {
	select {
	case m.duc <- dataUsageInfo{
		endpoint_name: endpoint,
		payloadSize:   data_stored,
		responseSize:  data_received,
	}:
		// data stored
	default:
		// channel full
		log.Debugf("Data Usage Channel full, dropping data")
	}
}

func (m *MetricsController) AggregateMetricData() map[string]metricsInfo {
	metricsMap := make(map[string]metricsInfo)

	// aggregate data from metrics channel
	//
	// Each metric can be aggregated as it is processed except for the failed
	// metrics. `failed` metrics are populated via the `NumFailedAttempts` function
	// call to the corresponding analytics event type, which is a running total of
	// the number of failed harvests until a success tracked in the
	// events.failedHarvests field. For that reason, we track the largest failed
	// value for a given event type rather than the sum of all failures to avoid
	// overcounting.
	loop := true
	for loop {
		select {
		case metric, ok := <-m.mc:
			if ok { // check that channel is not closed
				metrics := metricsMap[metric.eventType]
				metrics.seen += metric.seen
				metrics.sent += metric.sent
				if metric.failed > metrics.failed {
					metrics.failed = metric.failed
				}
				metricsMap[metric.eventType] = metrics
			}
		default:
			loop = false
		}
	}

	return metricsMap
}

func NewAppHarvest(id AgentRunID, app *App, harvest *Harvest, ph chan ProcessorHarvest) *AppHarvest {
	ah := &AppHarvest{
		App:           app,
		Harvest:       harvest,
		TraceObserver: nil,
		trigger:       make(chan HarvestType),
		cancel:        make(chan bool),
		MetricController: &MetricsController{
			mc:  make(chan metricsInfo, 64),
			duc: make(chan dataUsageInfo, 64),
			wg:  new(sync.WaitGroup),
		},
	}

	if len(app.info.TraceObserverHost) > 0 {
		cfg := &infinite_tracing.Config{
			RunId:             id.String(),
			License:           string(app.info.License),
			Host:              app.info.TraceObserverHost,
			Port:              app.info.TraceObserverPort,
			Secure:            true,
			QueueSize:         app.info.SpanQueueSize,
			RequestHeadersMap: app.connectReply.RequestHeadersMap,
		}
		ah.TraceObserver = infinite_tracing.NewTraceObserver(cfg)
	}

	// Start a goroutine to handle messages from the application's harvest trigger
	// function and send them onto the processor.
	go func() {
		for t := range ah.trigger {
			ph <- ah.NewProcessorHarvestEvent(id, t)
		}
	}()

	// Start the application's harvest trigger function in a goroutine.
	go app.HarvestTrigger(ah.trigger, ah.cancel)

	return ah
}

func (ah *AppHarvest) Close() error {
	ah.cancel <- true
	// Wait for confirmation that the cancellation has been processed before
	// closing the trigger.
	<-ah.cancel

	if ah.TraceObserver != nil {
		ah.TraceObserver.Shutdown(500 * time.Millisecond)
	}

	close(ah.trigger)
	close(ah.cancel)
	return nil
}
