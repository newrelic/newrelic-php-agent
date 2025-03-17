//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"encoding/json"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

// LogEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type LogEvents struct {
	*analyticsEvents
	LogForwardingLabels []LogForwardingLabel
}

// define a struct to represent the format of labels as sent by agent
type LogForwardingLabel struct {
	LabelType  string `json:"label_type"`
	LabelValue string `json:"label_value"`
}

// NewLogEvents returns a new Log event reservoir with capacity max.
func NewLogEvents(max int) *LogEvents {
	return &LogEvents{analyticsEvents: newAnalyticsEvents(max)}
}

// AddEventFromData observes the occurrence of an Log event. If the
// reservoir is full, sampling occurs. Note: when sampling occurs, it
// is possible the new event may be discarded.
func (events *LogEvents) AddEventFromData(data []byte, priority SamplingPriority) {
	events.AddEvent(AnalyticsEvent{data: data, priority: priority})
}

// AddLogForwardingLabels accepts JSON in the format used to send labels
// to the collector. This is used to add labels to the log events.  The
// labels are added to the log events when the events are sent to the
// collector.
func (events *LogEvents) SetLogForwardingLabels(data []byte) {
	err := json.Unmarshal(data, &events.LogForwardingLabels)
	if nil != err {
		log.Errorf("failed to unmarshal log labels json", err)
	}

	// verify valid labels
	for idx := range events.LogForwardingLabels {
		if len(events.LogForwardingLabels[idx].LabelType) == 0 || len(events.LogForwardingLabels[idx].LabelValue) == 0 {
			log.Errorf("invalid log label: %s log value: %s", events.LogForwardingLabels[idx].LabelType, events.LogForwardingLabels[idx].LabelValue)
			events.LogForwardingLabels = nil
			break
		}
	}
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
		`"common": {"attributes": `)
	nwrit := 0
	labelMap := make(map[string]string)
	for _, label := range events.LogForwardingLabels {
		// safegaurd against invalid labels
		if len(label.LabelType) != 0 && len(label.LabelValue) != 0 {
			labelMap["tags."+label.LabelType] = label.LabelValue
		}
	}

	j, e := json.Marshal(labelMap)
	if e != nil {
		log.Errorf("failed to marshal log label: %s", e)
		buf.WriteString("{}")
	} else {
		buf.Write(j)
	}

	buf.WriteString(`},` +
		`"logs": [`)

	nwrit = 0
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
