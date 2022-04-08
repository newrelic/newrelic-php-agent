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

func TestMarshalJSONEvent(t *testing.T) {
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

func TestUnmarshalJSONEventError(t *testing.T) {
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
		ehc := NewEventHarvestConfig(nil)
		if err := json.Unmarshal([]byte(input.json), &ehc); err == nil {
			t.Errorf("%s: unexpected success unmarshalling event_harvest_config JSON", input.name)
		}
	}
}

func TestUnmarshalJSONSpanEventError(t *testing.T) {
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
			json: `{"harvest_limit":-1}`,
		},
	} {
		sehc := NewDefaultSpanEventHarvestConfig()
		if err := json.Unmarshal([]byte(input.json), &sehc); err == nil {
			t.Errorf("%s: unexpected success unmarshalling span_event_harvest_config JSON, got %v", input.name, sehc)
		}
	}
}


func CreateEventConfig(reportPeriod time.Duration) EventConfigs {
	eventConfig := NewHarvestLimits(nil)

	eventConfig.ErrorEventConfig.ReportPeriod = reportPeriod
	eventConfig.AnalyticEventConfig.ReportPeriod = reportPeriod
	eventConfig.CustomEventConfig.ReportPeriod = reportPeriod
	eventConfig.SpanEventConfig.ReportPeriod = reportPeriod

	return eventConfig
}

func NewDefaultEventHarvestConfig() EventHarvestConfig {
	config := NewEventHarvestConfig(nil)

	config.EventConfigs = CreateEventConfig(config.ReportPeriod)

	return config
}

func NewSpanEventHarvestConfig(reportPeriod time.Duration, localLimit int) SpanEventHarvestConfig {

    rp:= reportPeriod
    // The report period cannot be less than 1 ms
    if reportPeriod > limits.DefaultReportPeriod || reportPeriod <= (1 * time.Millisecond) {
        rp = limits.DefaultReportPeriod
    }

    ll := localLimit
    if localLimit > limits.MaxSpanMaxEvents || localLimit < 0 {
        ll = limits.MaxSpanMaxEvents
    }
    return SpanEventHarvestConfig {
    		SpanEventConfig:
    		    Event {
                    ReportPeriod: rp,
                    Limit: ll,
    		    },
    	    }
}

func NewDefaultSpanEventHarvestConfig() SpanEventHarvestConfig {
    return NewSpanEventHarvestConfig(limits.DefaultReportPeriod, limits.MaxSpanMaxEvents)
}

func TestUnmarshalJSONEventSuccess(t *testing.T) {

	defaultReportPeriodMs := limits.DefaultReportPeriod

	// When reading the below expected values, note that NewEventHarvestConfig(nil)
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
		testUnmarshalJSONEventCaseSuccess(t, input.name, "no context", input.json, &EventHarvestConfig{}, &input.expected)

		ehc := NewEventHarvestConfig(nil)
		testUnmarshalJSONEventCaseSuccess(t, input.name, "default context", input.json, &ehc, &input.expected)

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
		testUnmarshalJSONEventCaseSuccess(t, input.name, "junk context", input.json, &ehc, &input.expected)
	}
}

func testUnmarshalJSONEventCaseSuccess(t *testing.T, testName, contextName, inputJSON string, context *EventHarvestConfig, expected *EventHarvestConfig) {
	if err := json.Unmarshal([]byte(inputJSON), &context); err != nil {
		t.Errorf("%s; %s: unexpected error unmarshalling event_harvest_config JSON: %v", testName, contextName, err)
	}
	if !reflect.DeepEqual(context, expected) {
		t.Errorf("%s; %s: unmarshalling JSON event_harvest_config items are not equal: expected %v; got %v", testName, contextName, expected, context)
	}
}


func TestUnmarshalJSONSpanEventSuccess(t *testing.T) {

	// When reading the below expected values, note that NewEventHarvestConfig(nil)
	// really means "use the builtin defaults".

	for _, input := range []struct {
		name     string
		json     string
		expected SpanEventHarvestConfig
	}{
		{
			name:     "empty JSON",
			json:     `{}`,
			expected: NewDefaultSpanEventHarvestConfig(),
		},
		{
			name:     "null report period only",
			json:     `{"report_period_ms":null}`,
			expected: NewDefaultSpanEventHarvestConfig(),
		},
		{
			name:     "zero report period only",
			json:     `{"report_period_ms":0}`,
			expected: NewDefaultSpanEventHarvestConfig(),
		},
		{
			name: "valid report period only",
			json: `{"report_period_ms":42}`,
			expected: NewSpanEventHarvestConfig(42 * time.Millisecond, limits.MaxSpanMaxEvents),
		},
		{
			name: "zero harvest limit",
			json: `{"harvest_limit":0}`,
			expected: NewSpanEventHarvestConfig(limits.DefaultReportPeriod, 0),
		},
		{
			name: "valid non-zero harvest limit",
			json: `{"harvest_limit":1234}`,
			expected: NewSpanEventHarvestConfig(limits.DefaultReportPeriod, 1234),
		},
		{
			name: "valid nonzero harvest limit and valid report period",
			json: `{"report_period_ms":75,"harvest_limit":1212}`,
			expected: NewSpanEventHarvestConfig(75 * time.Millisecond, 1212),
		},
	} {
		// The given context shouldn't matter, but let's test a few variations to be
		// safe.

		testUnmarshalJSONSpanEventCaseSuccess(t, input.name, "no context", input.json, &SpanEventHarvestConfig{}, &input.expected)

		sehc := NewDefaultSpanEventHarvestConfig()
		testUnmarshalJSONSpanEventCaseSuccess(t, input.name, "default context", input.json, &sehc, &input.expected)

		sehc = NewSpanEventHarvestConfig(1234 * time.Nanosecond, 40000)

		testUnmarshalJSONSpanEventCaseSuccess(t, input.name, "junk context", input.json, &sehc, &input.expected)
	}
}

func testUnmarshalJSONSpanEventCaseSuccess(t *testing.T, testName, contextName, inputJSON string, context *SpanEventHarvestConfig, expected *SpanEventHarvestConfig) {
	if err := json.Unmarshal([]byte(inputJSON), &context); err != nil {
		t.Errorf("%s; %s: unexpected error unmarshalling span_event_harvest_config JSON: %v", testName, contextName, err)
	}
	if !reflect.DeepEqual(context, expected) {

		t.Errorf("%s; %s: unmarshalling JSON span_event_harvset_config items are not equal: expected %v; got %v", testName, contextName, expected, context)
	}
}