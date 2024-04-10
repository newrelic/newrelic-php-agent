//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/collector"
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
	harvest.LogEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"v":6},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.LogEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"v":6},{},{}]`), priority: SamplingPriority(0.8)})
	harvest.LogEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"v":6},{},{}]`), priority: SamplingPriority(0.8)})

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
			LogEventConfig: collector.Event{
				Limit: 5,
			},
		},
	}
	// Harvest will fail and retry depending on the response code from the backend
	// The specs say create a metric ONLY IF the total attempts are greater than 1
	// which means only send the metric if the harvest failed at some point and needed collector
	// re-attempted

	// For PHP, only metricEvents, ErrorEvents, CustomEvents, TxnEvents, and SpanEvents are retried,
	// therefore only those will ever have a count higher than 0 failed attemps.  For more details, see:
	// The Data Collection Limits section of Collector-Response-Handling.md in the agent-specs.

	// TxnEvents will succeed on the first attempt and should not generate a metric

	// Have CustomEvents fail once
	harvest.CustomEvents.FailedHarvest(harvest)

	// Have ErrorEvents fail twice for a total of 2
	harvest.ErrorEvents.FailedHarvest(harvest)
	harvest.ErrorEvents.FailedHarvest(harvest)

	// Have SpanEvents fail three times for a total of 3
	harvest.SpanEvents.FailedHarvest(harvest)
	harvest.SpanEvents.FailedHarvest(harvest)
	harvest.SpanEvents.FailedHarvest(harvest)

	// Have LogEvents fail 4 times for a total of 4
	harvest.LogEvents.FailedHarvest(harvest)
	harvest.LogEvents.FailedHarvest(harvest)
	harvest.LogEvents.FailedHarvest(harvest)
	harvest.LogEvents.FailedHarvest(harvest)

	harvest.createFinalMetrics(limits, nil)

	var expectedJSON = `["12345",1447203720,1417136520,` +
		`[[{"name":"Instance/Reporting"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/Agent/Collector/custom_event_data/Attempts"},[1,0,0,0,0,0]],` + // Check for Connect attempt supportability metrics
		`[{"name":"Supportability/Agent/Collector/error_event_data/Attempts"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/Agent/Collector/log_event_data/Attempts"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Agent/Collector/span_event_data/Attempts"},[3,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/AnalyticEventData/HarvestLimit"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/CustomEventData/HarvestLimit"},[3,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ErrorEventData/HarvestLimit"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/LogEventData/HarvestLimit"},[5,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[1234,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[28,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[28,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Seen"},[48,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Sent"},[48,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSeen"},[24,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSent"},[24,0,0,0,0,0]]]]`

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
			LogEventConfig: collector.Event{
				Limit: 5,
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
		`[{"name":"Supportability/EventHarvest/LogEventData/HarvestLimit"},[5,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[1234,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Sent"},[0,0,0,0,0,0]],` +
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

	// verify that php packages does not send harvest when data is nil
	h = NewHarvest(startTime, collector.NewHarvestLimits(nil))
	h.PhpPackages.AddPhpPackagesFromData(nil)
	if !h.empty() {
		t.Errorf("Harvest.empty = false, want true")
	}

}
