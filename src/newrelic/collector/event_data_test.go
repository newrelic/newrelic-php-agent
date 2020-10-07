//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"encoding/json"
	"reflect"
	"testing"
	"time"

	"newrelic/limits"
)

func TestDurationToMillisecondsError(t *testing.T) {
	for _, d := range []time.Duration{
		-1,
		-1 * time.Nanosecond,
		-1 * time.Hour,
		-9223372036854775808,
	} {
		_, err := durationToMilliseconds(d)
		if err == nil {
			t.Errorf("expected an error and did not get one for input: %v", d)
		}
	}
}

func TestDurationToMillisecondsSuccess(t *testing.T) {
	for _, input := range []struct {
		duration time.Duration
		expected uint64
	}{
		{0, 0},
		{1 * time.Nanosecond, 0},
		{1 * time.Millisecond, 1},
		{1 * time.Second, 1000},
		{1 * time.Minute, 60000},
	} {
		ms, err := durationToMilliseconds(input.duration)
		if err != nil {
			t.Errorf("unexpected error for input %v: %v", input.duration, err)
		}
		if ms != input.expected {
			t.Errorf("unexpected value: expected %v; got %v", input.expected, ms)
		}
	}
}

func TestMarshalJSON(t *testing.T) {
	expected := `{"report_period_ms":20000,"harvest_limits":{"error_event_data":0,"analytic_event_data":0,"custom_event_data":0,"span_event_data":0}}`
	output, err := json.Marshal(EventHarvestConfig{ReportPeriod: 20 * time.Second})
	if err != nil {
		t.Errorf("unexpected error when marshalling a valid config: %v", err)
	}
	if string(output) != expected {
		t.Errorf("invalid output JSON: expected %v; got %v", expected, string(output))
	}

	if _, err := json.Marshal(EventHarvestConfig{ReportPeriod: -1}); err == nil {
		t.Errorf("unexpected success when marshalling an invalid config")
	}
}

func TestUnmarshalJSONError(t *testing.T) {
	for _, input := range []struct {
		name string
		json string
	}{
		{
			name: "negative report period",
			json: `{"report_period_ms":-1}`,
		},
		{
			name: "negative harvest limit",
			json: `{"harvest_limits":{"error_event_data":-1}}`,
		},
	} {
		ehc := NewEventHarvestConfig()
		if err := json.Unmarshal([]byte(input.json), &ehc); err == nil {
			t.Errorf("%s: unexpected success unmarshalling JSON", input.name)
		}
	}
}

func CreateEventConfig(reportPeriod time.Duration) EventConfigs {
	eventConfig := NewHarvestLimits()

	eventConfig.ErrorEventConfig.ReportPeriod = reportPeriod
	eventConfig.AnalyticEventConfig.ReportPeriod = reportPeriod
	eventConfig.CustomEventConfig.ReportPeriod = reportPeriod
	eventConfig.SpanEventConfig.ReportPeriod = reportPeriod

	return eventConfig
}

func NewDefaultEventHarvestConfig() EventHarvestConfig {
	config := NewEventHarvestConfig()

	config.EventConfigs = CreateEventConfig(config.ReportPeriod)

	return config
}

func TestUnmarshalJSONSuccess(t *testing.T) {

	defaultReportPeriodMs := limits.DefaultReportPeriod

	// When reading the below expected values, note that NewEventHarvestConfig()
	// really means "use the builtin defaults".
	for _, input := range []struct {
		name     string
		json     string
		expected EventHarvestConfig
	}{
		{
			name:     "empty JSON",
			json:     `{}`,
			expected: NewDefaultEventHarvestConfig(),
		},
		{
			name:     "null report period only",
			json:     `{"report_period_ms":null}`,
			expected: NewDefaultEventHarvestConfig(),
		},
		{
			name:     "zero report period only",
			json:     `{"report_period_ms":0}`,
			expected: NewDefaultEventHarvestConfig(),
		},
		{
			name: "valid report period only",
			json: `{"report_period_ms":42}`,
			expected: EventHarvestConfig{
				ReportPeriod: 42 * time.Millisecond,
				EventConfigs: CreateEventConfig(defaultReportPeriodMs),
			},
		},
		{
			name:     "empty harvest limits",
			json:     `{"harvest_limits":{}}`,
			expected: NewDefaultEventHarvestConfig(),
		},
		{
			name:     "null harvest limits",
			json:     `{"harvest_limits":{"error_event_data":null,"analytic_event_data":null,"custom_event_data":null,"span_event_data":null}}`,
			expected: NewDefaultEventHarvestConfig(),
		},
		{
			name: "zero harvest limits",
			json: `{"harvest_limits":{"error_event_data":0,"analytic_event_data":0,"custom_event_data":0,"span_event_data":0}}`,
			expected: EventHarvestConfig{
				ReportPeriod: defaultReportPeriodMs,
				EventConfigs: EventConfigs{
					ErrorEventConfig: Event {
						   Limit: 0,
						   ReportPeriod: defaultReportPeriodMs,
					},
					AnalyticEventConfig: Event{
						Limit: 0,
						ReportPeriod: defaultReportPeriodMs,
					},
					CustomEventConfig:   Event{
						Limit: 0,
						ReportPeriod: defaultReportPeriodMs,
					},
					SpanEventConfig:     Event{
						Limit: 0,
						ReportPeriod: defaultReportPeriodMs,
					},
				},
			},
		},
		{
			name: "valid harvest limits",
			json: `{"harvest_limits":{"error_event_data":21,"analytic_event_data":22,"custom_event_data":23,"span_event_data":24}}`,
			expected: EventHarvestConfig{
				ReportPeriod: defaultReportPeriodMs,
				EventConfigs: EventConfigs{
					ErrorEventConfig: Event{
						Limit: 21,
						ReportPeriod: defaultReportPeriodMs,
					},
					AnalyticEventConfig: Event{
						Limit: 22,
						ReportPeriod: defaultReportPeriodMs,
					},
					CustomEventConfig:   Event{
						Limit: 23,
						ReportPeriod: defaultReportPeriodMs,
					},
					SpanEventConfig:     Event{
						Limit: 24,
						ReportPeriod: defaultReportPeriodMs,
					},
				},
			},
		},
		{
			name: "valid harvest limits and report period",
			json: `{"report_period_ms":42,"harvest_limits":{"error_event_data":21,"analytic_event_data":22,"custom_event_data":23,"span_event_data":24}}`,
			expected: EventHarvestConfig{
				ReportPeriod: 42 * time.Millisecond,
				EventConfigs: EventConfigs{
					ErrorEventConfig:    Event{
						Limit: 21,
						ReportPeriod: 42 * time.Millisecond,
					},
					AnalyticEventConfig: Event{
						Limit: 22,
						ReportPeriod: 42 * time.Millisecond,
					},
					CustomEventConfig:   Event{
						Limit: 23,
						ReportPeriod: 42 * time.Millisecond,
					},
					SpanEventConfig:     Event{
						Limit: 24,
						ReportPeriod: 42 * time.Millisecond,
					},
				},
			},
		},
	} {
		// The given context shouldn't matter, but let's test a few variations to be
		// safe.
		testUnmarshalJSONCaseSuccess(t, input.name, "no context", input.json, &EventHarvestConfig{}, &input.expected)

		ehc := NewEventHarvestConfig()
		testUnmarshalJSONCaseSuccess(t, input.name, "default context", input.json, &ehc, &input.expected)

		ehc = EventHarvestConfig{
			ReportPeriod: 1234 * time.Nanosecond,
			EventConfigs: EventConfigs{
				ErrorEventConfig:    Event{
					Limit: 1,
				},
				AnalyticEventConfig: Event {
					Limit: 2,
				},
				CustomEventConfig:   Event{
					Limit: 3,
				},
				SpanEventConfig:     Event{
					Limit: 4,
				},
			},
		}
		testUnmarshalJSONCaseSuccess(t, input.name, "junk context", input.json, &ehc, &input.expected)
	}
}

func testUnmarshalJSONCaseSuccess(t *testing.T, testName, contextName, inputJSON string, context *EventHarvestConfig, expected *EventHarvestConfig) {
	if err := json.Unmarshal([]byte(inputJSON), &context); err != nil {
		t.Errorf("%s; %s: unexpected error unmarshalling JSON: %v", testName, contextName, err)
	}
	if !reflect.DeepEqual(context, expected) {
		t.Errorf("%s; %s: items are not equal: expected %v; got %v", testName, contextName, expected, context)
	}
}
