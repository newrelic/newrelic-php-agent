//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"strconv"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/collector"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/infinite_tracing"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/limits"
)

type AggregaterInto interface {
	AggregateInto(h *Harvest)
}

// HarvestCounters tracks cumulative event counts across harvest cycles.
// These counters are used to generate accurate TotalEventsSeen/Sent metrics
// even when custom harvest triggers fire at different intervals than the
// default harvest trigger.
type HarvestCounters struct {
	txnEventsSeen    float64
	txnEventsSent    float64
	customEventsSeen float64
	customEventsSent float64
	errorEventsSeen  float64
	errorEventsSent  float64
	spanEventsSeen   float64
	spanEventsSent   float64
	logEventsSeen    float64
	logEventsSent    float64
}

// Reset clears all counters back to zero. This should be called after
// HarvestDefaultData completes and the counters have been used to generate
// TotalEventsSeen/Sent metrics.
func (hc *HarvestCounters) Reset() {
	hc.txnEventsSeen = 0
	hc.txnEventsSent = 0
	hc.customEventsSeen = 0
	hc.customEventsSent = 0
	hc.errorEventsSeen = 0
	hc.errorEventsSent = 0
	hc.spanEventsSeen = 0
	hc.spanEventsSent = 0
	hc.logEventsSeen = 0
	hc.logEventsSent = 0
}

type Harvest struct {
	Metrics           *MetricTable
	Errors            *ErrorHeap
	SlowSQLs          *SlowSQLs
	TxnTraces         *TxnTraces
	TxnEvents         *TxnEvents
	CustomEvents      *CustomEvents
	ErrorEvents       *ErrorEvents
	SpanEvents        *SpanEvents
	LogEvents         *LogEvents
	PhpPackages       *PhpPackages
	commandsProcessed int
	pidSet            map[int]struct{}
	httpErrorSet      map[int]float64
	counters          HarvestCounters
}

func NewHarvest(now time.Time, hl collector.EventConfigs) *Harvest {
	nh := &Harvest{
		Metrics:           NewMetricTable(limits.MaxMetrics, now),
		Errors:            NewErrorHeap(limits.MaxErrors),
		SlowSQLs:          NewSlowSQLs(limits.MaxSlowSQLs),
		TxnTraces:         NewTxnTraces(),
		TxnEvents:         NewTxnEvents(hl.AnalyticEventConfig.Limit),
		CustomEvents:      NewCustomEvents(hl.CustomEventConfig.Limit),
		ErrorEvents:       NewErrorEvents(hl.ErrorEventConfig.Limit),
		SpanEvents:        NewSpanEvents(hl.SpanEventConfig.Limit),
		LogEvents:         NewLogEvents(hl.LogEventConfig.Limit),
		PhpPackages:       NewPhpPackages(),
		commandsProcessed: 0,
		pidSet:            make(map[int]struct{}),
		httpErrorSet:      make(map[int]float64),
	}

	return nh
}

func (h *Harvest) empty() bool {
	return len(h.pidSet) == 0 &&
		h.CustomEvents.Empty() &&
		h.ErrorEvents.Empty() &&
		h.SpanEvents.Empty() &&
		h.Errors.Empty() &&
		h.Metrics.Empty() &&
		h.SlowSQLs.Empty() &&
		h.TxnEvents.Empty() &&
		h.TxnTraces.Empty() &&
		h.LogEvents.Empty() &&
		h.PhpPackages.Empty()
}

func createTraceObserverMetrics(to *infinite_tracing.TraceObserver, metrics *MetricTable) {
	if to == nil {
		return
	}

	for name, val := range to.DumpSupportabilityMetrics() {
		metrics.AddRaw([]byte(name), "", "", val, Forced)
	}
}

func (h *Harvest) createHttpErrorMetrics() {
	if h.empty() {
		// No agent data received, do not create derived metrics. This allows
		// upstream to detect inactivity sooner.
		return
	}

	for code, val := range h.httpErrorSet {
		h.Metrics.AddCount("Supportability/Agent/Collector/HTTPError/"+strconv.Itoa(code), "", val, Forced)
	}
}

// Update the Http error counts
func (h *Harvest) IncrementHttpErrors(statusCode int) {
	if h.empty() {
		// No agent data received, do not create derived metrics. This allows
		// upstream to detect inactivity sooner.
		return
	}
	counter, isPresent := h.httpErrorSet[statusCode]

	if isPresent {
		h.httpErrorSet[statusCode] = counter + 1
	} else {
		h.httpErrorSet[statusCode] = 1
	}
}

func (h *Harvest) createEndpointAttemptsMetric(endpoint string, val float64) {
	if h.empty() {
		// No agent data received, do not create derived metrics. This allows
		// upstream to detect inactivity sooner.
		return
	}

	if val > 0 {
		h.Metrics.AddCount("Supportability/Agent/Collector/"+endpoint+"/Attempts", "", val, Forced)
	}

}

func (h *Harvest) createFinalMetrics(harvestLimits collector.EventHarvestConfig, to *infinite_tracing.TraceObserver) {
	if h.empty() {
		// No agent data received, do not create derived metrics. This allows
		// upstream to detect inactivity sooner.
		return
	}

	pidSetSize := len(h.pidSet)

	if 0 == pidSetSize {
		// For UI purposes, Instance/Reporting has to be nonzero.
		pidSetSize = 1
	}

	// NOTE: It is important that this metric be created once per harvest period.
	h.Metrics.AddCount("Instance/Reporting", "", float64(pidSetSize), Forced)

	// Custom Events Supportability Metrics
	// Use cumulative counters to avoid race conditions with custom harvest triggers
	h.Metrics.AddCount("Supportability/Events/Customer/Seen", "", h.counters.customEventsSeen, Forced)
	h.Metrics.AddCount("Supportability/Events/Customer/Sent", "", h.counters.customEventsSent, Forced)
	h.createEndpointAttemptsMetric(h.CustomEvents.Cmd(), h.CustomEvents.NumFailedAttempts())

	// Transaction Events Supportability Metrics
	// Note that these metrics used to have different names:
	//   Supportability/RequestSampler/requests
	//   Supportability/RequestSampler/samples
	// Use cumulative counters to avoid race conditions with custom harvest triggers
	h.Metrics.AddCount("Supportability/AnalyticsEvents/TotalEventsSeen", "", h.counters.txnEventsSeen, Forced)
	h.Metrics.AddCount("Supportability/AnalyticsEvents/TotalEventsSent", "", h.counters.txnEventsSent, Forced)
	h.createEndpointAttemptsMetric(h.TxnEvents.Cmd(), h.TxnEvents.NumFailedAttempts())

	// Error Events Supportability Metrics
	// Use cumulative counters to avoid race conditions with custom harvest triggers
	h.Metrics.AddCount("Supportability/Events/TransactionError/Seen", "", h.counters.errorEventsSeen, Forced)
	h.Metrics.AddCount("Supportability/Events/TransactionError/Sent", "", h.counters.errorEventsSent, Forced)
	h.createEndpointAttemptsMetric(h.ErrorEvents.Cmd(), h.ErrorEvents.NumFailedAttempts())

	if h.Metrics.numDropped > 0 {
		h.Metrics.AddCount("Supportability/MetricsDropped", "", float64(h.Metrics.numDropped), Forced)
	}

	// Span Events Supportability Metrics
	// Use cumulative counters to avoid race conditions with custom harvest triggers
	h.Metrics.AddCount("Supportability/SpanEvent/TotalEventsSeen", "", h.counters.spanEventsSeen, Forced)
	h.Metrics.AddCount("Supportability/SpanEvent/TotalEventsSent", "", h.counters.spanEventsSent, Forced)
	h.createEndpointAttemptsMetric(h.SpanEvents.Cmd(), h.SpanEvents.analyticsEvents.NumFailedAttempts())

	// Log Events Supportability Metrics
	// Use cumulative counters to avoid race conditions with custom harvest triggers
	h.Metrics.AddCount("Supportability/Logging/Forwarding/Seen", "", h.counters.logEventsSeen, Forced)
	h.Metrics.AddCount("Supportability/Logging/Forwarding/Sent", "", h.counters.logEventsSent, Forced)
	h.createEndpointAttemptsMetric(h.LogEvents.Cmd(), h.LogEvents.analyticsEvents.NumFailedAttempts())

	// Certificate supportability metrics.
	if collector.CertPoolState == collector.SystemCertPoolMissing {
		h.Metrics.AddCount("Supportability/PHP/SystemCertificates/Unavailable", "", float64(1), Forced)
	} else if collector.CertPoolState == collector.SystemCertPoolAvailable {
		h.Metrics.AddCount("Supportability/PHP/SystemCertificates/Available", "", float64(1), Forced)
	}
	h.Metrics.AddCount("Supportability/EventHarvest/ReportPeriod", "", float64(harvestLimits.ReportPeriod), Forced)
	h.Metrics.AddCount("Supportability/EventHarvest/AnalyticEventData/HarvestLimit", "", float64(harvestLimits.EventConfigs.AnalyticEventConfig.Limit), Forced)
	h.Metrics.AddCount("Supportability/EventHarvest/CustomEventData/HarvestLimit", "", float64(harvestLimits.EventConfigs.CustomEventConfig.Limit), Forced)
	h.Metrics.AddCount("Supportability/EventHarvest/ErrorEventData/HarvestLimit", "", float64(harvestLimits.EventConfigs.ErrorEventConfig.Limit), Forced)
	h.Metrics.AddCount("Supportability/EventHarvest/SpanEventData/HarvestLimit", "", float64(harvestLimits.EventConfigs.SpanEventConfig.Limit), Forced)
	h.Metrics.AddCount("Supportability/EventHarvest/LogEventData/HarvestLimit", "", float64(harvestLimits.EventConfigs.LogEventConfig.Limit), Forced)

	h.createEndpointAttemptsMetric(h.Metrics.Cmd(), h.Metrics.NumFailedAttempts())

	createTraceObserverMetrics(to, h.Metrics)

	h.createHttpErrorMetrics()
}

type FailedHarvestSaver interface {
	FailedHarvest(*Harvest)
}

type PayloadCreator interface {
	FailedHarvestSaver
	Empty() bool
	Data(id AgentRunID, harvestStart time.Time) ([]byte, error)
	// For many data types, the audit version is the same as the data. Those
	// data types return nil from Audit.
	Audit(id AgentRunID, harvestStart time.Time) ([]byte, error)
	Cmd() string
}

func (x *MetricTable) Cmd() string  { return collector.CommandMetrics }
func (x *CustomEvents) Cmd() string { return collector.CommandCustomEvents }
func (x *ErrorEvents) Cmd() string  { return collector.CommandErrorEvents }
func (x *SpanEvents) Cmd() string   { return collector.CommandSpanEvents }
func (x *LogEvents) Cmd() string    { return collector.CommandLogEvents }
func (x *ErrorHeap) Cmd() string    { return collector.CommandErrors }
func (x *SlowSQLs) Cmd() string     { return collector.CommandSlowSQLs }
func (x *TxnTraces) Cmd() string    { return collector.CommandTraces }
func (x *TxnEvents) Cmd() string    { return collector.CommandTxnEvents }
func (x *PhpPackages) Cmd() string  { return collector.CommandPhpPackages }

func IntegrationData(p PayloadCreator, id AgentRunID, harvestStart time.Time) ([]byte, error) {
	audit, err := p.Audit(id, harvestStart)
	if nil != err {
		return nil, err
	}
	if nil != audit {
		return audit, nil
	}
	return p.Data(id, harvestStart)
}
