//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"time"

	"newrelic/collector"
	"newrelic/limits"
	"newrelic/log"
)

// A harvest trigger function. Each App has one of these, and it sends
// HarvestType messages to the given channel to initiate a harvest for that app.
// When an AppHarvest is closed, it sends a `true` event over the trigger
// function's cancel channel.
type HarvestTriggerFunc func(trigger chan HarvestType, cancel chan bool)

// Given a reply, determine whether the configuration requires the same
// reporting period across all data reporting.
func (reply *ConnectReply) isHarvestAll() bool {
	if reply == nil {
		// If a well-formed ConnectReply is not supplied, assume a default reporting
		// period across all data reporting.
		return true
	}
	eventsConfig := reply.EventHarvestConfig.EventConfigs
	collectorReportPeriod := reply.EventHarvestConfig.ReportPeriod

	// If any event has a unique report period we will not be able to harvest
	// all events at the same time.
	if eventsConfig.ErrorEventConfig.ReportPeriod != collectorReportPeriod ||
		eventsConfig.AnalyticEventConfig.ReportPeriod != collectorReportPeriod ||
		eventsConfig.CustomEventConfig.ReportPeriod != collectorReportPeriod ||
		eventsConfig.SpanEventConfig.ReportPeriod != collectorReportPeriod {
			return false
	}

	return reply.EventHarvestConfig.ReportPeriod == limits.DefaultReportPeriod
}

// A convenience function to create a harvest trigger function which triggers
// a harvest event of type t once every duration.
func triggerBuilder(t HarvestType, duration time.Duration) HarvestTriggerFunc {
	return func(trigger chan HarvestType, cancel chan bool) {
		ticker := time.NewTicker(duration)
		for {
			select {
			case <-ticker.C:
				trigger <- t
			case <-cancel:
				ticker.Stop()
				// Send a message back to the cancel channel confirming that the
				// ticker has been stopped.
				cancel <- true
				return
			}
		}
	}
}

// To create a group of goroutines that may be cancelled by sending a single
// message on a single channel, this function:
// - Creates a cancel channel for the goroutine function f.
// - Starts the goroutine.
// - Returns the newly-created cancel channel so that it may be added to a
//   broadcast group.
func startGroupMember(f HarvestTriggerFunc, trigger chan HarvestType) chan bool {
	cancel := make(chan bool)
	go f(trigger, cancel)
	return cancel
}

func checkReportPeriod(period time.Duration, defaultPeriod time.Duration, event string) time.Duration{
	if period == 0 {
		log.Debugf("%s report period not received", event)
		return defaultPeriod
	} else {
		log.Debugf("setting %s report period to: %d", event, period)
		return period
	}
}

// In some cases, five different kinds of data are harvested at five different
// periods.  In such cases, build the comprehensive harvest trigger that adheres
// to such a configuration.
func customTriggerBuilder(reply *ConnectReply) HarvestTriggerFunc {
	defaultPeriod := limits.DefaultReportPeriod
	reportPeriod := reply.EventHarvestConfig.ReportPeriod
	eventConfig := reply.EventHarvestConfig.EventConfigs

	reportPeriod = checkReportPeriod(reportPeriod, defaultPeriod, "all events")
	eventConfig.AnalyticEventConfig.ReportPeriod = checkReportPeriod(
		eventConfig.AnalyticEventConfig.ReportPeriod, defaultPeriod, "Analytic Events")
	eventConfig.CustomEventConfig.ReportPeriod = checkReportPeriod(
		eventConfig.CustomEventConfig.ReportPeriod, defaultPeriod, "Custom Events")
	eventConfig.ErrorEventConfig.ReportPeriod = checkReportPeriod(
		eventConfig.ErrorEventConfig.ReportPeriod, defaultPeriod, "Error Events")
	eventConfig.SpanEventConfig.ReportPeriod = checkReportPeriod(
		eventConfig.SpanEventConfig.ReportPeriod, defaultPeriod, "Span Events")

	defaultTrigger := triggerBuilder(HarvestDefaultData, defaultPeriod)
	analyticTrigger := triggerBuilder(HarvestTxnEvents, eventConfig.AnalyticEventConfig.ReportPeriod)
	customTrigger := triggerBuilder(HarvestCustomEvents, eventConfig.CustomEventConfig.ReportPeriod)
	errorTrigger := triggerBuilder(HarvestErrorEvents, eventConfig.ErrorEventConfig.ReportPeriod)
	spanTrigger := triggerBuilder(HarvestSpanEvents, eventConfig.SpanEventConfig.ReportPeriod)

	return func(trigger chan HarvestType, cancel chan bool) {
		broadcastGroup := []chan bool{
			startGroupMember(defaultTrigger, trigger),
			startGroupMember(analyticTrigger, trigger),
			startGroupMember(customTrigger, trigger),
			startGroupMember(errorTrigger, trigger),
			startGroupMember(spanTrigger, trigger),
		}

		// This function listens for the cancel message and then broadcasts it
		// to all members of the broadcastGroup.
		go func() {
			<-cancel
			for _, c := range broadcastGroup {
				c <- true
				// As we need to send a confirmation that all trigger functions
				// have been cancelled, we'll wait for this function to confirm
				// that the cancellation has been processed.
				<-c
			}

			// Send a confirmation that the cancellation has been processed,
			// since we know from the loop above that all functions in the
			// broadcast group have been cancelled.
			cancel <- true
		}()
	}
}

// This function returns the harvest trigger function that should be used for
// this agent.  In priority order:
//   1. Either it uses the ConnectReply to build custom triggers as specified by
//      the New Relic server-side collector.
//   2. Or it creates a default harvest trigger, harvesting all data at the
//      default period.
func getHarvestTrigger(key collector.LicenseKey, reply *ConnectReply) HarvestTriggerFunc {
	// Build a trigger from the server-side collector configuration.
	if reply.isHarvestAll() {
		return triggerBuilder(HarvestAll, limits.DefaultReportPeriod)
	} else {
		return customTriggerBuilder(reply)
	}
}
