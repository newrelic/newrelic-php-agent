//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

// ErrorEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type ErrorEvents struct {
	*analyticsEvents
}

// NewErrorEvents returns a new error event reservoir with capacity max.
func NewErrorEvents(max int) *ErrorEvents {
	return &ErrorEvents{newAnalyticsEvents(max)}
}

// AddEventFromData observes the occurrence of an error event. If the
// reservoir is full, sampling occurs. Note: when sampling occurs, it
// is possible the new event may be discarded.
func (events *ErrorEvents) AddEventFromData(data []byte, priority SamplingPriority) {
	events.AddEvent(AnalyticsEvent{data: data, priority: priority})
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of events to the collector
// fails. After a failed delivery attempt, events is merged into
// the upcoming harvest. This may result in sampling.
func (events *ErrorEvents) FailedHarvest(newHarvest *Harvest) {
	newHarvest.ErrorEvents.MergeFailed(events.analyticsEvents)
}
