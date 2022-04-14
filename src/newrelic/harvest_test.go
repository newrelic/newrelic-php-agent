//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"
	"time"

	"newrelic/collector"
)

func TestCreateFinalMetricsWithLotsOfMetrics(t *testing.T) {
	harvest := NewHarvest(time.Date(2015, time.November, 11, 1, 2, 0, 0, time.UTC), collector.NewHarvestLimits(nil))

	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.SpanEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"w":7},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.SpanEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"w":7},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.SpanEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"w":7},{},{}]`), priority: SamplingPriority(0.8)})

	collector.CertPoolState = collector.SystemCertPoolIgnored
	limits := collector.EventHarvestConfig{
		ReportPeriod: 1234,
		EventConfigs: collector.EventConfigs{
			ErrorEventConfig: collector.Event{
				Limit: 1,
			},
			AnalyticEventConfig: collector.Event{
				Limit: 2,
			},
			CustomEventConfig: collector.Event{
				Limit: 3,
			},
			SpanEventConfig: collector.Event{
				Limit: 4,
			},
		},
	}
	harvest.createFinalMetrics(limits, nil)

	var expectedJSON = `["12345",1447203720,1417136520,` +
		`[[{"name":"Instance/Reporting"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/AnalyticEventData/HarvestLimit"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/CustomEventData/HarvestLimit"},[3,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ErrorEventData/HarvestLimit"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[1234,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[7,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[7,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSeen"},[3,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSent"},[3,0,0,0,0,0]]]]`

	json, err := harvest.Metrics.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if got := string(json); got != expectedJSON {
		t.Errorf("got=%q want=%q", got, expectedJSON)
	}
}

func TestCreateFinalMetricsWithNoMetrics(t *testing.T) {
	harvest := NewHarvest(time.Date(2015, time.November, 11, 1, 2, 0, 0, time.UTC), collector.NewHarvestLimits(nil))
	harvest.pidSet[0] = struct{}{}
	limits := collector.EventHarvestConfig{
		ReportPeriod: 1234,
		EventConfigs: collector.EventConfigs{
			ErrorEventConfig: collector.Event{
				Limit: 1,
			},
			AnalyticEventConfig: collector.Event{
				Limit: 2,
			},
			CustomEventConfig: collector.Event{
				Limit: 3,
			},
			SpanEventConfig: collector.Event{
				Limit: 4,
			},
		},
	}
	harvest.createFinalMetrics(limits, nil)

	var expectedJSON = `["12345",1447203720,1417136520,` +
		`[[{"name":"Instance/Reporting"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/AnalyticEventData/HarvestLimit"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/CustomEventData/HarvestLimit"},[3,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ErrorEventData/HarvestLimit"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[1234,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSent"},[0,0,0,0,0,0]]]]`

	json, err := harvest.Metrics.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if got := string(json); got != expectedJSON {
		t.Errorf("got=%q want=%q", got, expectedJSON)
	}
}

func TestHarvestEmpty(t *testing.T) {
	startTime := time.Date(2015, time.November, 11, 1, 2, 0, 0, time.UTC)

	if !NewHarvest(startTime, collector.NewHarvestLimits(nil)).empty() {
		t.Errorf("NewHarvest().empty() = false, want true")
	}

	var h *Harvest

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.pidSet[0] = struct{}{}
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.CustomEvents.AddEvent(AnalyticsEvent{priority: 0.42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.ErrorEvents.AddEvent(AnalyticsEvent{priority: 0.42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.Errors.AddError(51, []byte{}) /* Error priority = 51 */
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.Metrics.AddCount("WebTransaction", "", 1, Forced)
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.SlowSQLs.Observe(&SlowSQL{})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.TxnEvents.AddEvent(AnalyticsEvent{priority: 0.42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.TxnTraces.AddTxnTrace(&TxnTrace{DurationMillis: 42}) /* Transactions traces are sampled by duration */
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}
}
