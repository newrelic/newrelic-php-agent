//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"newrelic/infinite_tracing"
	"time"
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

	trigger chan HarvestType
	cancel  chan bool
}

func (ah *AppHarvest) NewProcessorHarvestEvent(id AgentRunID, t HarvestType) ProcessorHarvest {
	return ProcessorHarvest{
		AppHarvest: ah,
		ID:         id,
		Type:       t,
	}
}

func NewAppHarvest(id AgentRunID, app *App, harvest *Harvest, ph chan ProcessorHarvest) *AppHarvest {
	ah := &AppHarvest{
		App:           app,
		Harvest:       harvest,
		TraceObserver: nil,
		trigger:       make(chan HarvestType),
		cancel:        make(chan bool),
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
