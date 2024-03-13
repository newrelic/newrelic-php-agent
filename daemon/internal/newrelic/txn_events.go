//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

// TxnEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type TxnEvents struct {
	*analyticsEvents
}

// NewTxnEvents returns a new transaction event reservoir with capacity max.
func NewTxnEvents(max int) *TxnEvents {
	return &TxnEvents{newAnalyticsEvents(max)}
}

// AddTxnEvent observes the occurrence of a transaction event. If the
// reservoir is full, sampling occurs. Note: when sampling occurs, it
// is possible the new event may be discarded.
func (events *TxnEvents) AddTxnEvent(data []byte, priority SamplingPriority) {
	events.AddEvent(AnalyticsEvent{data: data, priority: priority})
}

// AddSyntheticsEvent observes the occurrence of a Synthetics
// transaction event. If the reservoir is full, sampling occurs. Note:
// when sampling occurs, it is possible the new event may be
// discarded.
func (events *TxnEvents) AddSyntheticsEvent(data []byte, priority SamplingPriority) {
	// Synthetics events always get priority: normal event priorities are in the
	// range [0.0,1.99999], so adding 2 means that a Synthetics event will always
	// win.
	events.AddEvent(AnalyticsEvent{data: data, priority: 2 + priority})
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of events to the collector
// fails. After a failed delivery attempt, events is merged into
// the upcoming harvest. This may result in sampling.
func (events *TxnEvents) FailedHarvest(newHarvest *Harvest) {
	newHarvest.TxnEvents.MergeFailed(events.analyticsEvents)
}
