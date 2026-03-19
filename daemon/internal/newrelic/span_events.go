//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

// SpanEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type SpanEvents struct {
	*analyticsEvents
}

// NewSpanEvents returns a new span event reservoir with capacity max.
func NewSpanEvents(max int) *SpanEvents {
	return &SpanEvents{newAnalyticsEvents(max)}
}

// AddEventFromData observes the occurrence of a span event. If the
// reservoir is full, sampling occurs. Note: when sampling occurs, it
// is possible the new event may be discarded.
func (events *SpanEvents) AddEventFromData(data []byte, priority SamplingPriority) {
	events.AddEvent(AnalyticsEvent{data: data, priority: priority})
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of events to the collector
// fails. After a failed delivery attempt, events is merged into
// the upcoming harvest. This may result in sampling.
func (events *SpanEvents) FailedHarvest(newHarvest *Harvest) {
	newHarvest.SpanEvents.MergeFailed(events.analyticsEvents)
}
