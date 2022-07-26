//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"encoding/json"
	"fmt"
	"time"

	"newrelic/limits"
	"newrelic/log"
)

// rawHarvestLimits represents the harvest_limits object specified in connect
// messages. We use pointers to distinguish between nil and 0.
type rawHarvestLimits struct {
	ErrorEventData    *int `json:"error_event_data"`
	AnalyticEventData *int `json:"analytic_event_data"`
	CustomEventData   *int `json:"custom_event_data"`
	SpanEventData     *int `json:"span_event_data"`
}

// rawEventHarvestConfig is the wire representation of the event_harvest_config,
// as opposed to the unmarshalled view that we provide to the rest of the
// daemon in EventConfigs.
type rawEventHarvestConfig struct {
	ReportPeriodMS uint64           `json:"report_period_ms"`
	HarvestLimits  rawHarvestLimits `json:"harvest_limits"`
}

// rawSpanEventHarvestConfig is the wire representation of the span_event_harvest_config,
// as opposed to the unmarshalled view that we provide to the rest of the
// daemon in EventConfigs.  Since span events are not included in Faster Event Harvest
// due to concerns about downsampling within a distributed trace, the report period and
// harvest limit are reported separately in span_event_harvest_config instead of
// event_harvest_config; HOWEVER, they are still eventually added to EventConfigs for the daemon
// to use.
type rawSpanEventHarvestConfig struct {
	ReportPeriodMS uint64 `json:"report_period_ms"`
	SpanEventData  *int   `json:"harvest_limit"`
}

// Event lets you specify the limit and report period for each event type, this
// is used in EventConfigs.
type Event struct {
	Limit        int
	ReportPeriod time.Duration
}

// EventConfigs represents the local harvest limits and harvest interval that
// were created based on what the collector returned in the event_harvest_config
// and the span_event_harvest_config.
type EventConfigs struct {
	ErrorEventConfig    Event
	AnalyticEventConfig Event
	CustomEventConfig   Event
	SpanEventConfig     Event
	LogEventConfig      Event
}

// EventHarvestConfig represents the event_harvest_config object used by the daemon
// specified in connect messages.
type EventHarvestConfig struct {
	// This is used to determine if harvestAll can be used. It is also used by
	// a supportability metric.
	ReportPeriod time.Duration
	EventConfigs EventConfigs
}

// EventHarvestConfig holds the span_event_harvest_config object
// specified in connect messages.
type SpanEventHarvestConfig struct {
	SpanEventConfig Event
}

// durationToMilliseconds converts a report period duration to a raw number of
// milliseconds. As report period durations can never be negative,
// durationToMilliseconds will return an error if the given duration is
// negative.
func durationToMilliseconds(d time.Duration) (uint64, error) {
	if d < 0 {
		return 0, fmt.Errorf("report period cannot be negative: %v", d)
	}

	return uint64(d / time.Millisecond), nil
}

// MarshalJSON marshals an EventHarvestConfig into (effectively) a marshalled
// rawEventHarvestConfig, which is the wire format expected by the collector.
func (daemonConfig EventHarvestConfig) MarshalJSON() ([]byte, error) {
	ms, err := durationToMilliseconds(daemonConfig.ReportPeriod)
	if err != nil {
		return nil, err
	}

	// The daemon event_harvest_config contains per-event report periods that
	// were inferred based on the connect message. We need to remove these for
	// the collector.
	collectorConfig := rawHarvestLimits{
		ErrorEventData:    &daemonConfig.EventConfigs.ErrorEventConfig.Limit,
		AnalyticEventData: &daemonConfig.EventConfigs.AnalyticEventConfig.Limit,
		CustomEventData:   &daemonConfig.EventConfigs.CustomEventConfig.Limit,
		SpanEventData:     &daemonConfig.EventConfigs.SpanEventConfig.Limit,
	}

	rawConfig := rawEventHarvestConfig{
		ReportPeriodMS: ms,
		HarvestLimits:  collectorConfig,
	}

	return json.Marshal(rawConfig)
}

// getEventConfig handles the logic of parsing the limits sent by the collector.
// If a limit was not given those events should have the default report period and
// the default limit.
func getEventConfig(rawLimit *int, collectorRate time.Duration, defaultLimit int, defaultRate time.Duration) (limit int, period time.Duration, err error) {
	if rawLimit == nil {
		limit = defaultLimit
		period = defaultRate
	} else if *rawLimit < 0 {
		err = fmt.Errorf("getEventConfig: event limit negative %d", rawLimit)
		return 0, 0, err
	} else {
		if *rawLimit > defaultLimit {
			limit = defaultLimit
		} else {
			limit = *rawLimit
		}
		period = collectorRate
	}
	return limit, period, nil
}

// UnmarshalJSON unmarshals a span_harvest_config JSON object into an
// SpanEventHarvestConfig.
func (daemonConfig *SpanEventHarvestConfig) UnmarshalJSON(b []byte) error {
	// Get the default report period in milliseconds.
	reportPeriodMs, err := durationToMilliseconds(limits.DefaultReportPeriod)
	if err != nil {
		return err
	}

	// Build a new struct that represents what we'll get from the collector with
	// the defaults (this is to ensure there is some value if the collector provides
	// invalid or incomplete JSONs).
	defaultLimit := limits.MaxSpanMaxEvents
	rawConfig := rawSpanEventHarvestConfig{
		ReportPeriodMS: reportPeriodMs,
		SpanEventData:  &defaultLimit,
	}

	// Unmarshal the JSON into the new struct.
	if err := json.Unmarshal(b, &rawConfig); err != nil {
		return err
	}

	// Validate the reporting period, since it cannot be zero, then copy it in as
	// a time.Duration.
	if rawConfig.ReportPeriodMS == 0 {
		log.Warnf("Unexpected report period of %d ms received; ignoring and using the default %v instead", rawConfig.ReportPeriodMS, limits.DefaultReportPeriod)
		daemonConfig.SpanEventConfig.ReportPeriod = limits.DefaultReportPeriod
	} else {
		daemonConfig.SpanEventConfig.ReportPeriod = time.Duration(rawConfig.ReportPeriodMS) * time.Millisecond
	}

	var harvestConfig Event

	// Check the span event value to see what the report period and limit should be.
	// getEventConfig will do additional data verification.
	harvestConfig.Limit,
		harvestConfig.ReportPeriod,
		err = getEventConfig(
		rawConfig.SpanEventData,
		daemonConfig.SpanEventConfig.ReportPeriod,
		limits.MaxSpanMaxEvents,
		limits.DefaultReportPeriod)
	if err != nil {
		log.Infof("Unexpected negative Span event limit %d", rawConfig.SpanEventData)
		return err
	}

	// Copy the harvest limits in.
	daemonConfig.SpanEventConfig = harvestConfig

	return nil
}

// UnmarshalJSON unmarshals an event_harvest_config JSON object into an
// EventHarvestConfig.
func (daemonConfig *EventHarvestConfig) UnmarshalJSON(b []byte) error {
	// Get the default report period in milliseconds.
	reportPeriodMs, err := durationToMilliseconds(limits.DefaultReportPeriod)
	if err != nil {
		return err
	}

	// Build a new struct that represents what we'll get from the collector with
	// the correct defaults.
	rawConfig := rawEventHarvestConfig{
		ReportPeriodMS: reportPeriodMs,
	}

	// Unmarshal the JSON into the new struct.
	if err := json.Unmarshal(b, &rawConfig); err != nil {
		return err
	}

	// Validate the reporting period, since it cannot be zero, then copy it in as
	// a time.Duration.
	if rawConfig.ReportPeriodMS == 0 {
		log.Warnf("Unexpected report period of %d ms received; ignoring and using the default %v instead", rawConfig.ReportPeriodMS, limits.DefaultReportPeriod)
		daemonConfig.ReportPeriod = limits.DefaultReportPeriod
	} else {
		daemonConfig.ReportPeriod = time.Duration(rawConfig.ReportPeriodMS) * time.Millisecond
	}

	rawLimits := rawConfig.HarvestLimits
	var harvestConfig EventConfigs

	// Check each event value to see what the report period and limit should be.
	harvestConfig.ErrorEventConfig.Limit,
		harvestConfig.ErrorEventConfig.ReportPeriod,
		err = getEventConfig(
		rawLimits.ErrorEventData,
		daemonConfig.ReportPeriod,
		limits.MaxErrorEvents,
		limits.DefaultReportPeriod)
	if err != nil {
		log.Infof("Unexpected negative error event limit %d", rawLimits.ErrorEventData)
		return err
	}

	harvestConfig.AnalyticEventConfig.Limit,
		harvestConfig.AnalyticEventConfig.ReportPeriod,
		err = getEventConfig(
		rawLimits.AnalyticEventData,
		daemonConfig.ReportPeriod,
		limits.MaxTxnEvents,
		limits.DefaultReportPeriod)
	if err != nil {
		log.Infof("Unexpected negative Analytic event limit %d", rawLimits.AnalyticEventData)
		return err
	}

	harvestConfig.CustomEventConfig.Limit,
		harvestConfig.CustomEventConfig.ReportPeriod,
		err = getEventConfig(
		rawLimits.CustomEventData,
		daemonConfig.ReportPeriod,
		limits.MaxCustomEvents,
		limits.DefaultReportPeriod)
	if err != nil {
		log.Infof("Unexpected negative Custom event limit %d", rawLimits.CustomEventData)
		return err
	}

	harvestConfig.SpanEventConfig.Limit,
		harvestConfig.SpanEventConfig.ReportPeriod,
		err = getEventConfig(
		rawLimits.SpanEventData,
		daemonConfig.ReportPeriod,
		limits.MaxSpanMaxEvents,
		limits.DefaultReportPeriod)
	if err != nil {
		log.Infof("Unexpected negative Span event limit %d", rawLimits.SpanEventData)
		return err
	}

	// Copy the harvest limits in.
	daemonConfig.EventConfigs = harvestConfig

	return nil
}

// NewHarvestLimits creates a EventConfigs with the correct default limits. The
// collector should not know about our per-event report period.  If we get
// per app config limits from the agent(s), we need to honor that.
// It takes an EventConfig variable so that if the agent has configured
// any limits, we can also incorporate those here.
func NewHarvestLimits(agentLimits *EventConfigs) EventConfigs {

	// Check if we have agent limits to incorporate.
	// Currently only max span events is configurable via the agent.
	spanEventLimit := limits.MaxSpanMaxEvents
	if agentLimits != nil {
		if (agentLimits.SpanEventConfig.Limit < limits.MaxSpanMaxEvents) &&
			(agentLimits.SpanEventConfig.Limit >= 0) {
			spanEventLimit = agentLimits.SpanEventConfig.Limit
		}
	}

	return EventConfigs{
		ErrorEventConfig: Event{
			Limit: limits.MaxErrorEvents,
		},
		AnalyticEventConfig: Event{
			Limit: limits.MaxTxnEvents,
		},
		CustomEventConfig: Event{
			Limit: limits.MaxCustomEvents,
		},
		SpanEventConfig: Event{
			Limit: spanEventLimit,
		},
	}
}

// NewEventHarvestConfig creates an event harvest configuration with the
// default values baked into the daemon. The collector should always receive
// the daemon default values. We should never send back what we received from
// the collector. We should never send per-event report periods.
// It takes an EventConfig variable so that if the agent has configured
// any limits, we can also incorporate those here.
func NewEventHarvestConfig(agentLimits *EventConfigs) EventHarvestConfig {
	return EventHarvestConfig{
		ReportPeriod: limits.DefaultReportPeriod,
		EventConfigs: NewHarvestLimits(agentLimits),
	}
}
