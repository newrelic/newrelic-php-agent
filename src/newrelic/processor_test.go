//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"testing"
	"time"

	"newrelic/collector"
	"newrelic/log"
	"newrelic/utilization"
)

var ErrPayloadTooLarge = errors.New("payload too large")
var ErrUnauthorized = errors.New("unauthorized")
var ErrUnsupportedMedia = errors.New("unsupported media")

type rpmException struct {
	Message   string `json:"message"`
	ErrorType string `json:"error_type"`
}

func (e *rpmException) Error() string {
	return fmt.Sprintf("%s: %s", e.ErrorType, e.Message)
}

const (
	forceRestartType   = "NewRelic::Agent::ForceRestartException"
	disconnectType     = "NewRelic::Agent::ForceDisconnectException"
	licenseInvalidType = "NewRelic::Agent::LicenseException"
	runtimeType        = "RuntimeError"
)

// These clients exist for testing.
var (
	DisconnectClient = collector.ClientFn(func(cmd collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
		return collector.RPMResponse{Body: nil, Err: SampleDisonnectException, StatusCode: 410}
	})
	LicenseInvalidClient = collector.ClientFn(func(cmd collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
		return collector.RPMResponse{Body: nil, Err: SampleLicenseInvalidException, StatusCode: 401}
	})
	SampleRestartException        = &rpmException{ErrorType: forceRestartType}
	SampleDisonnectException      = &rpmException{ErrorType: disconnectType}
	SampleLicenseInvalidException = &rpmException{ErrorType: licenseInvalidType}
)

var (
	idOne = AgentRunID("one")
	idTwo = AgentRunID("two")

	data        = JSONString(`{"age":29}`)
	encoded     = `"eJyqVkpMT1WyMrKsBQQAAP//EVgDDw=="`
	sampleTrace = &TxnTrace{Data: data}

	sampleCustomEvent = []byte("half birthday")
	sampleSpanEvent   = []byte("belated birthday")
	sampleLogEvent    = []byte("log event test birthday")
	sampleErrorEvent  = []byte("forgotten birthday")
)

type ClientReturn struct {
	reply []byte
	err   error
	code  int
}

type ClientParams struct {
	name string
	data []byte
}

type MockedProcessor struct {
	processorHarvestChan chan ProcessorHarvest
	clientReturn         chan ClientReturn
	clientParams         chan ClientParams
	p                    *Processor
}

func NewMockedProcessor(numberOfHarvestPayload int) *MockedProcessor {
	processorHarvestChan := make(chan ProcessorHarvest)
	clientReturn := make(chan ClientReturn, numberOfHarvestPayload)
	clientParams := make(chan ClientParams, numberOfHarvestPayload)

	client := collector.ClientFn(func(cmd collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
		data, err := cs.Collectible.CollectorJSON(false)
		if nil != err {
			return collector.RPMResponse{Err: err}
		}
		clientParams <- ClientParams{cmd.Name, data}
		r := <-clientReturn
		return collector.RPMResponse{Body: r.reply, Err: r.err, StatusCode: r.code}
	})

	p := NewProcessor(ProcessorConfig{Client: client})
	p.processorHarvestChan = processorHarvestChan
	p.trackProgress = make(chan struct{})
	p.appConnectBackoff = 0
	go p.Run()
	<-p.trackProgress // Wait for utilization

	return &MockedProcessor{
		processorHarvestChan: processorHarvestChan,
		clientReturn:         clientReturn,
		clientParams:         clientParams,
		p:                    p,
	}
}

func (m *MockedProcessor) DoAppInfoCustom(t *testing.T, id *AgentRunID, expectState AppState, info *AppInfo) {
	reply := m.p.IncomingAppInfo(id, info)
	<-m.p.trackProgress // receive app info
	if reply.State != expectState {
		t.Fatal(reply, expectState)
	}
}

func (m *MockedProcessor) DoAppInfo(t *testing.T, id *AgentRunID, expectState AppState) {
	m.DoAppInfoCustom(t, id, expectState, &sampleAppInfo)
}

func (m *MockedProcessor) DoConnect(t *testing.T, id *AgentRunID) {
	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{[]byte(`{"redirect_host":"specific_collector.com"}`), nil, 200}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{[]byte(`{"agent_run_id":"` + id.String() + `","zip":"zap","event_harvest_config":{"report_period_ms":5000,"harvest_limits":{"analytics_event_data":5,"custom_event_data":5,"error_event_data":5,"span_event_data":5,"log_event_data":5}}}`), nil, 200}
	<-m.p.trackProgress // receive connect reply
}

func (m *MockedProcessor) DoConnectConfiguredReply(t *testing.T, reply string) {
	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{[]byte(`{"redirect_host":"specific_collector.com"}`), nil, 200}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{[]byte(reply), nil, 200}
	<-m.p.trackProgress // receive connect reply
}

func (m *MockedProcessor) TxnData(t *testing.T, id AgentRunID, sample AggregaterInto) {
	m.p.IncomingTxnData(id, sample)
	<-m.p.trackProgress
}

type AggregaterIntoFn func(*Harvest)

func (fn AggregaterIntoFn) AggregateInto(h *Harvest) { fn(h) }

var (
	txnEventSample1 = AggregaterIntoFn(func(h *Harvest) {
		h.TxnEvents.AddTxnEvent([]byte(`[{"x":1},{},{}]`), SamplingPriority(0.8))
	})
	txnEventSample2 = AggregaterIntoFn(func(h *Harvest) {
		h.TxnEvents.AddTxnEvent([]byte(`[{"x":2},{},{}]`), SamplingPriority(0.8))
	})
	txnTraceSample = AggregaterIntoFn(func(h *Harvest) {
		h.TxnTraces.AddTxnTrace(sampleTrace)
	})
	txnCustomEventSample = AggregaterIntoFn(func(h *Harvest) {
		h.CustomEvents.AddEventFromData(sampleCustomEvent, SamplingPriority(0.8))
	})
	txnErrorEventSample = AggregaterIntoFn(func(h *Harvest) {
		h.ErrorEvents.AddEventFromData(sampleErrorEvent, SamplingPriority(0.8))
	})
	txnSpanEventSample = AggregaterIntoFn(func(h *Harvest) {
		h.SpanEvents.AddEventFromData(sampleSpanEvent, SamplingPriority(0.8))
	})
	txnLogEventSample = AggregaterIntoFn(func(h *Harvest) {
		h.LogEvents.AddEventFromData(sampleLogEvent, SamplingPriority(0.8))
	})
	txnEventSample1Times = func(times int) AggregaterIntoFn {
		return AggregaterIntoFn(func(h *Harvest) {
			for i := 0; i < times; i++ {
				h.TxnEvents.AddTxnEvent([]byte(`[{"x":1},{},{}]`), SamplingPriority(0.8))
			}
		})
	}
)

func TestProcessorHarvestDefaultData(t *testing.T) {
	m := NewMockedProcessor(2)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnTraceSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}

	// this code path will trigger two `harvestPayload` calls, so we need
	// to pluck two items out of the clientParams channels
	cp := <-m.clientParams
	cp2 := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice

	toTest := `["one",[[0,0,"","",` + encoded + `,"",null,false,null,null]]]`

	if string(cp.data) != toTest {
		if string(cp2.data) != toTest {
			t.Fatal(string(append(cp.data, cp2.data...)))
		}
	}

	m.p.quit()
}

func TestProcessorHarvestCustomEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnCustomEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestCustomEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	expected := `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]`
	if string(cp.data) != expected {
		t.Fatalf("expected: %s \ngot: %s", expected, string(cp.data))
	}

	m.p.quit()
}

func TestProcessorHarvestLogEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnLogEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestLogEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	expected := `["one",{"reservoir_size":5,"events_seen":1},[log event test birthday]]`
	if string(cp.data) != expected {
		t.Fatalf("expected: %s \ngot: %s", expected, string(cp.data))
	}

	m.p.quit()
}

func TestProcessorHarvestCleanExit(t *testing.T) {
	m := NewMockedProcessor(20)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnCustomEventSample)

	// Although only an event was inserted in this test, CleanExit triggers a path of execution
	// that eventually makes its way to Harvest's createFinalMetrics.  In this function, various
	// supportability and reporting metrics are added
	m.clientReturn <- ClientReturn{} /* metrics */
	m.clientReturn <- ClientReturn{} /* events */

	m.p.CleanExit()

	<-m.clientParams /* ditch metrics */
	cp := <-m.clientParams

	expected := `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]`
	if string(cp.data) != expected {
		t.Fatalf("expected: %s \ngot: %s", expected, string(cp.data))
	}
}

func TestSupportabilityHarvest(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnErrorEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}
	<-m.p.trackProgress              // receive harvest notice
	m.clientReturn <- ClientReturn{} /* metrics */
	//<-m.p.trackProgress // receive harvest

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	// Add timeout error response code for second harvest
	m.clientReturn <- ClientReturn{nil, ErrUnsupportedMedia, 408}
	<-m.p.trackProgress // receive harvest error

	harvest := m.p.harvests[idOne]
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
	// Because MockedProcessor wraps a real processor, we have no way to directly set the time
	//   of harvests. So we extract the time from what we receive
	time := strings.Split(string(cp.data), ",")[1]
	var expectedJSON = `["one",` + time + `,1417136520,` +
		`[[{"name":"Instance/Reporting"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/Agent/Collector/HTTPError/408"},[1,0,0,0,0,0]],` + // Check for HTTPError Supportability metric
		`[{"name":"Supportability/Agent/Collector/metric_data/Attempts"},[1,0,0,0,0,0]],` + //	Metrics were sent first when the 408 error occurred, so check for the metric failure.
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/AnalyticEventData/HarvestLimit"},[10002,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/CustomEventData/HarvestLimit"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ErrorEventData/HarvestLimit"},[6,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/LogEventData/HarvestLimit"},[10,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[5000001234,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/LogEvent/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/LogEvent/TotalEventsSent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSent"},[0,0,0,0,0,0]]]]`

	json, err := harvest.Metrics.CollectorJSONSorted(AgentRunID(idOne), end)
	if nil != err {
		t.Fatal(err)
	}
	if got := string(json); got != expectedJSON {
		t.Errorf("\ngot=%q \nwant=%q", got, expectedJSON)
	}
	m.p.quit()
}

func TestProcessorHarvestErrorEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnErrorEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestErrorEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":5,"events_seen":1},[forgotten birthday]]` {
		t.Fatal(string(cp.data))
	}

	m.p.quit()
}

func TestProcessorHarvestSpanEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnectConfiguredReply(t, `{"agent_run_id":"`+idOne.String()+`","zip":"zap","span_event_harvest_config":{"report_period_ms":5000,"harvest_limit":7},"event_harvest_config":{"report_period_ms":5000,"harvest_limits":{"analytics_event_data":5,"custom_event_data":5,"error_event_data":0,"span_event_data":5}}}`)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnSpanEventSample)
	m.TxnData(t, idOne, txnSpanEventSample)

	// Now we'll force a harvest for a span event type, and make sure we
	// receive that harvest.
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestSpanEvents,
	}

	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice, two span events in the data
	if string(cp.data) != `["one",{"reservoir_size":7,"events_seen":2},[belated birthday,belated birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.p.quit()

}

func TestProcessorHarvestSpanEventsZeroReservoir(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnectConfiguredReply(t, `{"agent_run_id":"`+idOne.String()+`","zip":"zap","span_event_harvest_config":{"report_period_ms":5000,"harvest_limit":0},"event_harvest_config":{"report_period_ms":5000,"harvest_limits":{"analytics_event_data":5,"custom_event_data":5,"error_event_data":0,"span_event_data":5}}}`)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnSpanEventSample)
	m.TxnData(t, idOne, txnSpanEventSample)
	m.TxnData(t, idOne, txnCustomEventSample)

	// Trigger a span event harvest. Due to the span_event_data limit being
	// zero, no harvest should actually occur here.

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestSpanEvents,
	}

	// No check of m.clientParams here because we expect no harvest to occur
	// due to the zero error_event_data limit.
	<-m.p.trackProgress // receive harvest notice

	// Now we'll force a harvest for a different event type, and make sure we
	// receive that harvest (and not a span event harvest).
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestCustomEvents,
	}

	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.p.quit()

}

func TestProcessorHarvestSpanEventsExceedReservoir(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnectConfiguredReply(t, `{"agent_run_id":"`+idOne.String()+`","zip":"zap","span_event_harvest_config":{"report_period_ms":5000,"harvest_limit":1},"event_harvest_config":{"report_period_ms":5000,"harvest_limits":{"analytics_event_data":5,"custom_event_data":5,"error_event_data":0,"span_event_data":5}}}`)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnSpanEventSample)
	m.TxnData(t, idOne, txnSpanEventSample)

	// Now we'll force a harvest for a span event type, and make sure we
	// receive that harvest.
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestSpanEvents,
	}

	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice with 2 span events seen, but only one span sent
	if string(cp.data) != `["one",{"reservoir_size":1,"events_seen":2},[belated birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.p.quit()

}

func TestProcessorHarvestZeroErrorEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnectConfiguredReply(t, `{"agent_run_id":"`+idOne.String()+`","zip":"zap","span_event_harvest_config":{"report_period_ms":5000,"harvest_limit":7},"event_harvest_config":{"report_period_ms":5000,"harvest_limits":{"analytics_event_data":5,"custom_event_data":5,"error_event_data":0,"span_event_data":5}}}`)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnErrorEventSample)
	m.TxnData(t, idOne, txnCustomEventSample)

	// Trigger an error event harvest. Due to the error_event_data limit being
	// zero, no harvest will actually occur here.
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestErrorEvents,
	}
	// No check of m.clientParams here because we expect no harvest to occur
	// due to the zero error_event_data limit.
	<-m.p.trackProgress // receive harvest notice

	// Now we'll force a harvest for a different event type, and make sure we
	// receive that harvest (and not an error event harvest).
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestCustomEvents,
	}

	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.p.quit()

}

func TestProcessorHarvestSplitTxnEvents(t *testing.T) {
	getEventsSeen := func(data []byte) float64 {
		var dataDec []interface{}

		err := json.Unmarshal(data, &dataDec)
		if nil != err {
			t.Fatal("Invalid JSON:", string(data))
		}

		metadata := dataDec[1].(map[string]interface{})
		return metadata["events_seen"].(float64)
	}

	// Distributed tracing deactivated.
	// --------------------------------
	// The event payload should never be split.

	m := NewMockedProcessor(1)

	appInfo := sampleAppInfo
	appInfo.Settings = map[string]interface{}{"newrelic.distributed_tracing_enabled": false}

	m.DoAppInfoCustom(t, nil, AppStateUnknown, &appInfo)
	m.DoConnect(t, &idOne)
	m.DoAppInfoCustom(t, nil, AppStateConnected, &appInfo)

	// 9000 events, no split.
	m.TxnData(t, idOne, txnEventSample1Times(9000))
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp1 := <-m.clientParams
	<-m.p.trackProgress
	cp1Events := getEventsSeen(cp1.data)
	if cp1Events != 9000 {
		t.Fatal("Expected 9000 events")
	}

	m.p.quit()

	// Distributed tracing activated.
	// ------------------------------
	// The event payload should be split for more than 4999 events.

	m = NewMockedProcessor(2)

	appInfo = sampleAppInfo
	appInfo.Settings = map[string]interface{}{"newrelic.distributed_tracing_enabled": true}

	m.DoAppInfoCustom(t, nil, AppStateUnknown, &appInfo)
	m.DoConnect(t, &idOne)
	m.DoAppInfoCustom(t, nil, AppStateConnected, &appInfo)

	// Less than 5000 events, no split.
	m.TxnData(t, idOne, txnEventSample1Times(4999))
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp1 = <-m.clientParams
	<-m.p.trackProgress
	cp1Events = getEventsSeen(cp1.data)
	if cp1Events != 4999 {
		t.Fatal("Expected 4999 events")
	}

	// 5000 events. Split into two payloads of 2500 each.
	m.TxnData(t, idOne, txnEventSample1Times(5000))
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp1 = <-m.clientParams
	cp2 := <-m.clientParams
	<-m.p.trackProgress
	cp1Events = getEventsSeen(cp1.data)
	cp2Events := getEventsSeen(cp2.data)
	if cp1Events != 2500 {
		t.Fatal("Payload with 2500 events expected, got ", cp1Events)
	}
	if cp2Events != 2500 {
		t.Fatal("Payload with 2500 events expected, got ", cp2Events)
	}

	// 8001 events. Split into two payloads of 4000 and 4001.
	// We do not know which payload arrives first.
	m.TxnData(t, idOne, txnEventSample1Times(8001))
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp1 = <-m.clientParams
	cp2 = <-m.clientParams
	<-m.p.trackProgress
	cp1Events = getEventsSeen(cp1.data)
	cp2Events = getEventsSeen(cp2.data)
	if cp1Events != 4000 && cp2Events != 4000 {
		t.Fatal("Payloads with 4000 events expected, got ", cp1Events, " and ", cp2Events)
	}
	if (cp1Events + cp2Events) != 8001 {
		t.Fatal("Payload sum of 8001 events expected, got ", cp1Events, " and ", cp2Events)
	}

	m.p.quit()
}

func TestForceRestart(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.clientReturn <- ClientReturn{nil, SampleRestartException, 401}
	<-m.p.trackProgress // receive harvest error

	m.DoConnect(t, &idTwo)

	m.DoAppInfo(t, &idOne, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)
	m.TxnData(t, idTwo, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idTwo],
		ID:         idTwo,
		Type:       HarvestTxnEvents,
	}

	<-m.p.trackProgress // receive harvest notice
	cp = <-m.clientParams
	if string(cp.data) != `["two",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.p.quit()
}

func TestDisconnectAtPreconnect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.p.quit()
}

func TestLicenseExceptionAtPreconnect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{nil, SampleLicenseInvalidException, 401}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateInvalidLicense)

	m.p.quit()
}

func TestDisconnectAtConnect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{[]byte(`{"redirect_host":"specific_collector.com"}`), nil, 200}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.p.quit()
}

func TestDisconnectAtHarvest(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	// Harvest both (final supportability) metrics and events to trigger
	// multiple calls to processHarvestError, the second call will have an
	// outdated run id.
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestAll,
	}
	<-m.p.trackProgress // receive harvest notice

	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // receive harvest error

	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // receive harvest error

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.p.quit()
}

func TestLicenseExceptionAtHarvest(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.clientReturn <- ClientReturn{nil, SampleLicenseInvalidException, 401}
	<-m.p.trackProgress // receive harvest error

	// Unknown app state triggered immediately following AppStateRestart
	m.DoAppInfo(t, nil, AppStateUnknown)

	m.p.quit()
}

func TestMalformedConnectReply(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{[]byte(`{"redirect_host":"specific_collector.com"}`), nil, 200}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{[]byte(`{`), nil, 202}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.p.quit()
}

func TestMalformedCollector(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{[]byte(`"`), nil, 200}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.p.quit()
}

func TestDataSavedOnHarvestError(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, errors.New("unusual error"), 500}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // receive harvest error

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp = <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
}

func TestNoDataSavedOnPayloadTooLarge(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, ErrPayloadTooLarge, 413}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // receive harvest error

	m.TxnData(t, idOne, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp = <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
}

func TestNoDataSavedOnErrUnsupportedMedia(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, ErrUnsupportedMedia, 415}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // receive harvest error

	m.TxnData(t, idOne, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp = <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
}

var (
	id      = AgentRunID("12345")
	otherId = AgentRunID("67890")

	sampleAppInfo = AppInfo{
		License:           collector.LicenseKey("12342352345"),
		Appname:           "Application",
		AgentLanguage:     "c",
		AgentVersion:      "0.0.1",
		Settings:          map[string]interface{}{},
		Environment:       nil,
		Labels:            nil,
		RedirectCollector: "collector.newrelic.com",
		HighSecurity:      true,
		Hostname:          "agent-hostname",
	}
	connectClient = collector.ClientFn(func(cmd collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
		if cmd.Name == collector.CommandPreconnect {
			return collector.RPMResponse{Body: []byte(`{"redirect_host":"specific_collector.com"}`), Err: nil, StatusCode: 202}
		}
		return collector.RPMResponse{Body: []byte(`{"agent_run_id":"12345","zip":"zap"}`), Err: nil, StatusCode: 200}
	})
)

func init() {
	log.Init(log.LogAlways, "stdout") // Avoid ssl mismatch warning
}

func TestAppInfoInvalid(t *testing.T) {
	p := NewProcessor(ProcessorConfig{Client: LicenseInvalidClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid || reply.ConnectTimestamp != 0 || reply.HarvestFrequency != 0 || reply.SamplingTarget != 0 {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	reply = p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateInvalidLicense || reply.ConnectReply != nil || reply.RunIDValid || reply.ConnectTimestamp != 0 || reply.HarvestFrequency != 0 || reply.SamplingTarget != 0 {
		t.Fatal(reply)
	}
	p.quit()
}

func TestAppInfoDisconnected(t *testing.T) {
	p := NewProcessor(ProcessorConfig{Client: DisconnectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid || reply.ConnectTimestamp != 0 || reply.HarvestFrequency != 0 || reply.SamplingTarget != 0 {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	reply = p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateDisconnected || reply.ConnectReply != nil || reply.RunIDValid || reply.ConnectTimestamp != 0 || reply.HarvestFrequency != 0 || reply.SamplingTarget != 0 {
		t.Fatal(reply)
	}
	p.quit()
}

func TestAppInfoConnected(t *testing.T) {
	p := NewProcessor(ProcessorConfig{Client: connectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid || reply.ConnectTimestamp != 0 || reply.HarvestFrequency != 0 || reply.SamplingTarget != 0 {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	// without agent run id
	reply = p.IncomingAppInfo(nil, &sampleAppInfo)
	if reply.State != AppStateConnected ||
		string(reply.ConnectReply) != `{"agent_run_id":"12345","zip":"zap"}` ||
		reply.RunIDValid ||
		reply.ConnectTimestamp == 0 ||
		reply.HarvestFrequency != 60 ||
		reply.SamplingTarget != 10 {
		t.Fatal(reply)
	}
	// with agent run id
	reply = p.IncomingAppInfo(&id, &sampleAppInfo)
	if !reply.RunIDValid {
		t.Fatal(reply)
	}

	p.quit()
}

func TestAppInfoRunIdOutOfDate(t *testing.T) {
	p := NewProcessor(ProcessorConfig{Client: connectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	badID := AgentRunID("bad_id")
	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	reply = p.IncomingAppInfo(&badID, &sampleAppInfo)
	if reply.State != AppStateConnected || string(reply.ConnectReply) != `{"agent_run_id":"12345","zip":"zap"}` ||
		reply.RunIDValid {
		t.Fatal(reply)
	}
	p.quit()
}

func TestAppInfoDifferentHostname(t *testing.T) {
	p := NewProcessor(ProcessorConfig{Client: connectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// Connect with the default sample application info.

	info := sampleAppInfo

	reply := p.IncomingAppInfo(&id, &info)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	// Connect with the same application info, but a different host name.
	// This must trigger another connect request.

	info.Hostname = fmt.Sprintf("%s-2", info.Hostname)

	reply = p.IncomingAppInfo(&otherId, &info)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	p.quit()
}

func TestShouldConnect(t *testing.T) {
	p := NewProcessor(ProcessorConfig{Client: connectClient})
	now := time.Now()
	p.appConnectBackoff = 2 * time.Second

	if p.util != nil {
		t.Error("Utilization should be nil until connected.")
	}

	if p.shouldConnect(&App{state: AppStateUnknown}, now) {
		t.Error("Shouldn't connect app if utilzation data is nil.")
	}

	p.util = &utilization.Data{}
	if !p.shouldConnect(&App{state: AppStateUnknown}, now) {
		t.Error("Should connect app if timeout is valid and app is unknown.")
	}
	if p.shouldConnect(&App{state: AppStateUnknown, lastConnectAttempt: now}, now) ||
		p.shouldConnect(&App{state: AppStateUnknown, lastConnectAttempt: now}, now.Add(time.Second)) {
		t.Error("Shouldn't connect app if last connect attempt was too recent.")
	}
	if !p.shouldConnect(&App{state: AppStateUnknown, lastConnectAttempt: now}, now.Add(3*time.Second)) {
		t.Error("Should connect app if timeout is small enough.")
	}
	if p.shouldConnect(&App{state: AppStateConnected, lastConnectAttempt: now}, now.Add(3*time.Second)) {
		t.Error("Shouldn't connect app if app is already connected.")
	}
}
