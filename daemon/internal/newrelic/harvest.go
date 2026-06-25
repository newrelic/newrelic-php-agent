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
	for code, val := range h.httpErrorSet {
		h.Metrics.AddCount("Supportability/Agent/Collector/HTTPError/"+strconv.Itoa(code), "", val, Forced)
	}
}

// Update the Http error counts
func (h *Harvest) IncrementHttpErrors(statusCode int) {
	counter, isPresent := h.httpErrorSet[statusCode]

	if isPresent {
		h.httpErrorSet[statusCode] = counter + 1
	} else {
		h.httpErrorSet[statusCode] = 1
	}
}

func (h *Harvest) createEndpointAttemptsMetric(endpoint string, val float64) {
	if val > 0 {
		h.Metrics.AddCount("Supportability/Agent/Collector/"+endpoint+"/Attempts", "", val, Forced)
	}

}

// NOTE: It is important that this metric be created once per harvest period.
func (h *Harvest) addInstanceReportingMetric() {
	pidSetSize := len(h.pidSet)
	if pidSetSize == 0 {
		// For UI purposes, Instance/Reporting has to be nonzero.
		pidSetSize = 1
	}
	h.Metrics.AddCount("Instance/Reporting", "", float64(pidSetSize), Forced)
}

func (h *Harvest) createFinalMetrics(harvestLimits collector.EventHarvestConfig, to *infinite_tracing.TraceObserver, mc *MetricsController) {
	if len(mc.duc) == 0 && len(mc.mc) == 0 {
		// No agent data received, do not create derived metrics. This allows
		// upstream to detect inactivity sooner.
		return
	}

	if h.Metrics.numDropped > 0 {
		h.Metrics.AddCount("Supportability/MetricsDropped", "", float64(h.Metrics.numDropped), Forced)
	}

	// Certificate supportability metrics.
	switch collector.CertPoolState {
	case collector.SystemCertPoolMissing:
		h.Metrics.AddCount("Supportability/PHP/SystemCertificates/Unavailable", "", float64(1), Forced)
	case collector.SystemCertPoolAvailable:
		h.Metrics.AddCount("Supportability/PHP/SystemCertificates/Available", "", float64(1), Forced)
	default:
	}

	metricsMap := mc.AggregateMetricData()

	// Custom Events Supportability Metrics
	customEventMetrics := metricsMap[collector.CommandCustomEvents]
	h.Metrics.AddCount("Supportability/Events/Customer/Seen", "", customEventMetrics.seen, Forced)
	h.Metrics.AddCount("Supportability/Events/Customer/Sent", "", customEventMetrics.sent, Forced)
	h.createEndpointAttemptsMetric(h.CustomEvents.Cmd(), customEventMetrics.failed)

	// Transaction Events Supportability Metrics
	transactionEventMetrics := metricsMap[collector.CommandTxnEvents]
	h.Metrics.AddCount("Supportability/AnalyticsEvents/TotalEventsSeen", "", transactionEventMetrics.seen, Forced)
	h.Metrics.AddCount("Supportability/AnalyticsEvents/TotalEventsSent", "", transactionEventMetrics.sent, Forced)
	h.createEndpointAttemptsMetric(h.TxnEvents.Cmd(), transactionEventMetrics.failed)

	// Error Events Supportability Metrics
	errorEventMetrics := metricsMap[collector.CommandErrorEvents]
	h.Metrics.AddCount("Supportability/Events/TransactionError/Seen", "", errorEventMetrics.seen, Forced)
	h.Metrics.AddCount("Supportability/Events/TransactionError/Sent", "", errorEventMetrics.sent, Forced)
	h.createEndpointAttemptsMetric(h.ErrorEvents.Cmd(), errorEventMetrics.failed)

	// Span Events Supportability Metrics
	spanEventMetrics := metricsMap[collector.CommandSpanEvents]
	h.Metrics.AddCount("Supportability/SpanEvent/TotalEventsSeen", "", spanEventMetrics.seen, Forced)
	h.Metrics.AddCount("Supportability/SpanEvent/TotalEventsSent", "", spanEventMetrics.sent, Forced)
	h.createEndpointAttemptsMetric(h.SpanEvents.Cmd(), spanEventMetrics.failed)

	// Log Events Supportability Metrics
	logEventMetrics := metricsMap[collector.CommandLogEvents]
	h.Metrics.AddCount("Supportability/Logging/Forwarding/Seen", "", logEventMetrics.seen, Forced)
	h.Metrics.AddCount("Supportability/Logging/Forwarding/Sent", "", logEventMetrics.sent, Forced)
	h.createEndpointAttemptsMetric(h.LogEvents.Cmd(), logEventMetrics.failed)

	// Harvest Limit and report period metrics
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
