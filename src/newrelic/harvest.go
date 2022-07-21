//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"strconv"
	"time"

	"newrelic/collector"
	"newrelic/infinite_tracing"
	"newrelic/limits"
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
	commandsProcessed int
	pidSet            map[int]struct{}
	httpErrorSet      map[int]float64
}

func NewHarvest(now time.Time, hl collector.EventConfigs) *Harvest {
	return &Harvest{
		Metrics:           NewMetricTable(limits.MaxMetrics, now),
		Errors:            NewErrorHeap(limits.MaxErrors),
		SlowSQLs:          NewSlowSQLs(limits.MaxSlowSQLs),
		TxnTraces:         NewTxnTraces(),
		TxnEvents:         NewTxnEvents(hl.AnalyticEventConfig.Limit),
		CustomEvents:      NewCustomEvents(hl.CustomEventConfig.Limit),
		ErrorEvents:       NewErrorEvents(hl.ErrorEventConfig.Limit),
		SpanEvents:        NewSpanEvents(hl.SpanEventConfig.Limit),
		commandsProcessed: 0,
		pidSet:            make(map[int]struct{}),
		httpErrorSet:      make(map[int]float64),
	}
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
		h.LogEvents.Empty()
}

func createTraceObserverMetrics(to *infinite_tracing.TraceObserver, metrics *MetricTable) {
	if to == nil {
		return
	}

	for name, val := range to.DumpSupportabilityMetrics() {
		metrics.AddCount(name, "", val, Forced)
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
	h.Metrics.AddCount("Supportability/Events/Customer/Seen", "", h.CustomEvents.NumSeen(), Forced)
	h.Metrics.AddCount("Supportability/Events/Customer/Sent", "", h.CustomEvents.NumSaved(), Forced)
	h.createEndpointAttemptsMetric(h.CustomEvents.Cmd(), h.CustomEvents.NumFailedAttempts())

	// Transaction Events Supportability Metrics
	// Note that these metrics used to have different names:
	//   Supportability/RequestSampler/requests
	//   Supportability/RequestSampler/samples

	h.Metrics.AddCount("Supportability/AnalyticsEvents/TotalEventsSeen", "", h.TxnEvents.NumSeen(), Forced)
	h.Metrics.AddCount("Supportability/AnalyticsEvents/TotalEventsSent", "", h.TxnEvents.NumSaved(), Forced)
	h.createEndpointAttemptsMetric(h.TxnEvents.Cmd(), h.TxnEvents.NumFailedAttempts())

	// Error Events Supportability Metrics
	h.Metrics.AddCount("Supportability/Events/TransactionError/Seen", "", h.ErrorEvents.NumSeen(), Forced)
	h.Metrics.AddCount("Supportability/Events/TransactionError/Sent", "", h.ErrorEvents.NumSaved(), Forced)
	h.createEndpointAttemptsMetric(h.ErrorEvents.Cmd(), h.ErrorEvents.NumFailedAttempts())

	if h.Metrics.numDropped > 0 {
		h.Metrics.AddCount("Supportability/MetricsDropped", "", float64(h.Metrics.numDropped), Forced)
	}

	// Span Events Supportability Metrics
	h.Metrics.AddCount("Supportability/SpanEvent/TotalEventsSeen", "", h.SpanEvents.analyticsEvents.NumSeen(), Forced)
	h.Metrics.AddCount("Supportability/SpanEvent/TotalEventsSent", "", h.SpanEvents.analyticsEvents.NumSaved(), Forced)
	h.createEndpointAttemptsMetric(h.SpanEvents.Cmd(), h.SpanEvents.analyticsEvents.NumFailedAttempts())

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
func (x *ErrorHeap) Cmd() string    { return collector.CommandErrors }
func (x *SlowSQLs) Cmd() string     { return collector.CommandSlowSQLs }
func (x *TxnTraces) Cmd() string    { return collector.CommandTraces }
func (x *TxnEvents) Cmd() string    { return collector.CommandTxnEvents }

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
