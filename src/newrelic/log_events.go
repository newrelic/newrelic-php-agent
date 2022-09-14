//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"time"
)

// LogEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type LogEvents struct {
	*analyticsEvents
}

// NewLogEvents returns a new Log event reservoir with capacity max.
func NewLogEvents(max int) *LogEvents {
	return &LogEvents{newAnalyticsEvents(max)}
}

// AddEventFromData observes the occurrence of an Log event. If the
// reservoir is full, sampling occurs. Note: when sampling occurs, it
// is possible the new event may be discarded.
func (events *LogEvents) AddEventFromData(data []byte, priority SamplingPriority) {
	events.AddEvent(AnalyticsEvent{data: data, priority: priority})
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of events to the collector
// fails. After a failed delivery attempt, events is merged into
// the upcoming harvest. This may result in sampling.
func (events *LogEvents) FailedHarvest(newHarvest *Harvest) {
	newHarvest.LogEvents.MergeFailed(events.analyticsEvents)
}

// CollectorJSON marshals events to JSON according to the schema expected
// by the collector.
func (events *LogEvents) CollectorJSON(id AgentRunID) ([]byte, error) {
	buf := &bytes.Buffer{}

	es := *events.analyticsEvents.events

	estimate := len(es) * 128
	buf.Grow(estimate)
	buf.WriteString(`[{` +
		`"common": {"attributes": {}},` +
		`"logs": [`)

	nwrit := 0
	for i := 0; i < len(es); i++ {
		// if obviously incomplete skip
		if len(es[i].data) < 4 {
			continue
		}
		if nwrit > 0 {
			buf.WriteByte(',')
		}
		nwrit++
		buf.Write(es[i].data)
	}
	buf.WriteByte(']')
	buf.WriteByte('}')
	buf.WriteByte(']')

	return buf.Bytes(), nil
}

// Data marshals the collection to JSON according to the schema expected
// by the collector.
func (events *LogEvents) Data(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return events.CollectorJSON(id)
}

// Audit marshals the collection to JSON according to the schema
// expected by the audit log. For analytics events, the audit schema is
// the same as the schema expected by the collector.
func (events *LogEvents) Audit(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return nil, nil
}
