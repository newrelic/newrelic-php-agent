//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"
)

type LogForwardingLabelsTestCase struct {
	name     string
	labels   string
	expected string
}

var (
	logForwardingLabelsTestCases = []LogForwardingLabelsTestCase{
		LogForwardingLabelsTestCase{name: "Valid JSON",
			labels:   `[{"label_type":"type1","label_value":"value1"},{"label_type":"type2","label_value":"value2"}]`,
			expected: `{"tags.type1":"value1","tags.type2":"value2"}`},
		LogForwardingLabelsTestCase{name: "Invalid JSON", labels: `NOT VALID JSON`, expected: `{}`},
		LogForwardingLabelsTestCase{name: "Empty labels 1", labels: ``, expected: `{}`},
		LogForwardingLabelsTestCase{name: "Empty labels 2", labels: `[]`, expected: `{}`},
		LogForwardingLabelsTestCase{name: "Empty labels 3", labels: `[{}]`, expected: `{}`},
		LogForwardingLabelsTestCase{name: "Invalid keys", labels: `[{"label_tipe":"type1","label_valyue":"value1"}]`, expected: `{}`},
		LogForwardingLabelsTestCase{name: "Space in value", labels: `[{"label_type":"type1","label_value":"value 1"}]`, expected: `{"tags.type1":"value 1"}`},
		LogForwardingLabelsTestCase{name: "Space in key", labels: `[{"label_type":"type 1","label_value":"value1"}]`, expected: `{"tags.type 1":"value1"}`},
		LogForwardingLabelsTestCase{name: "Empty value", labels: `[{"label_type":"type1","label_value":""}]`, expected: `{}`},
		LogForwardingLabelsTestCase{name: "Empty key", labels: `[{"label_type":"","label_value":"value1"}]`, expected: `{}`},
	}
)

// LogEvents is a wrapper over AnalyticsEvents created for additional type
// There are already unit tests for AnalyticsEvents in analytics_events_test.go
// These tests will focus on the methods specific to LogEvents

func TestAddEventFromData(t *testing.T) {
	events := NewLogEvents(10)
	id := AgentRunID(`12345`)
	data := []byte(`{"message":"test log event"}`)
	priority := SamplingPriority(0.5)

	events.AddEventFromData(data, priority)

	if events.analyticsEvents.events.Len() != 1 {
		t.Errorf("expected 1 event, got %d", events.analyticsEvents.events.Len())
	}

	es := *events.analyticsEvents.events
	event := es[0]

	if string(event.data) != string(data) {
		t.Errorf("expected event data %s, got %s", string(data), string(event.data))
	}

	if event.priority != priority {
		t.Errorf("expected event priority %f, got %f", priority, event.priority)
	}

	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}

	expected := `[{"common": {"attributes": {}},"logs": [{"message":"test log event"}]}]`
	if string(json) != expected {
		t.Errorf("expected JSON %s, got %s", expected, string(json))
	}
}

func TestSetLogForwardingLabels(t *testing.T) {

	for idx := range logForwardingLabelsTestCases {
		testcase := logForwardingLabelsTestCases[idx]
		events := NewLogEvents(10)
		id := AgentRunID(`12345`)
		log_data := []byte(`{"message":"test log event"}`)
		label_data := []byte(testcase.labels)
		priority := SamplingPriority(0.5)

		events.AddEventFromData(log_data, priority)
		events.SetLogForwardingLabels(label_data)

		if events.analyticsEvents.events.Len() != 1 {
			t.Errorf("%s: expected 1 event, got %d", testcase.name, events.analyticsEvents.events.Len())
		}

		json, err := events.CollectorJSON(id)
		if nil != err {
			t.Fatal(err)
		}

		expected := `[{"common": {"attributes": ` + testcase.expected + `},` +
			`"logs": [{"message":"test log event"}]}]`
		if string(json) != expected {
			t.Errorf("%s: expected JSON %s, got %s", testcase.name, expected, string(json))
		}
	}
}

func TestSetLogForwardingLabelsNilData(t *testing.T) {
	events := NewLogEvents(10)
	id := AgentRunID(`12345`)
	log_data := []byte(`{"message":"test log event"}`)
	priority := SamplingPriority(0.5)

	events.AddEventFromData(log_data, priority)
	events.SetLogForwardingLabels(nil)

	if events.analyticsEvents.events.Len() != 1 {
		t.Errorf("expected 1 event, got %d", events.analyticsEvents.events.Len())
	}

	es := *events.analyticsEvents.events
	event := es[0]

	if string(event.data) != string(log_data) {
		t.Errorf("expected event data %s, got %s", string(log_data), string(event.data))
	}

	if event.priority != priority {
		t.Errorf("expected event priority %f, got %f", priority, event.priority)
	}

	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}

	expected := `[{"common": {"attributes": {}},` +
		`"logs": [{"message":"test log event"}]}]`
	if string(json) != expected {
		t.Errorf("expected JSON %s, got %s", expected, string(json))
	}
}
