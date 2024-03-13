//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/collector"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/utilization"
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
	DisconnectClient = collector.ClientFn(func(cmd *collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
		return collector.RPMResponse{Body: nil, Err: SampleDisonnectException, StatusCode: 410}
	})
	LicenseInvalidClient = collector.ClientFn(func(cmd *collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
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
	samplePhpPackages = []byte(`["package", "1.2.3",{}]`)
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

	client := collector.ClientFn(func(cmd *collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
		data, err := cs.Collectible.CollectorJSON(false)
		cmd.Data = data
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

func (m *MockedProcessor) QuitTestProcessor() {
	// If the test took a while to run, it's possible that the processor is
	// blocking, as a new harvest was triggered and then the trackProgress
	// channel is waiting to be consumed
	select {
	case <-m.p.trackProgress:
		// receive harvest notice
	default:
		// nothing on channel
	}
	m.p.quit()
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
	txnPhpPackagesSample = AggregaterIntoFn(func(h *Harvest) {
		h.PhpPackages.AddPhpPackagesFromData(samplePhpPackages)
	})
	txnEventSample1Times = func(times int) AggregaterIntoFn {
		return AggregaterIntoFn(func(h *Harvest) {
			for i := 0; i < times; i++ {
				h.TxnEvents.AddTxnEvent([]byte(`[{"x":1},{},{}]`), SamplingPriority(0.8))
			}
		})
	}
)

func NewAppInfoWithLogEventLimit(limit int) AppInfo {
	appInfo := sampleAppInfo
	appInfo.AgentEventLimits = collector.EventConfigs{
		LogEventConfig: collector.Event{
			Limit:        limit,
			ReportPeriod: 5000,
		},
	}

	return appInfo
}

func NewAppInfoWithCustomEventLimit(limit int) AppInfo {
	appInfo := sampleAppInfo
	appInfo.AgentEventLimits = collector.EventConfigs{
		CustomEventConfig: collector.Event{
			Limit:        limit,
			ReportPeriod: 5000,
		},
	}

	return appInfo
}

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

	// this code path will trigger three `harvestPayload` calls, so we need
	// to pluck three items out of the clientParams channels
	/* collect txn */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp := <-m.clientParams
	/* collect metrics */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp2 := <-m.clientParams
	/* collect usage metrics */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp3 := <-m.clientParams

	<-m.p.trackProgress // unblock processor after harvest

	toTest := `["one",[[0,0,"","",` + encoded + `,"",null,false,null,null]]]`

	if string(cp.data) != toTest {
		if string(cp2.data) != toTest {
			t.Fatal(string(append(cp.data, cp2.data...)))
		}
	}
	time1 := strings.Split(string(cp3.data), ",")[1]
	time2 := strings.Split(string(cp3.data), ",")[2]
	usageMetrics := `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[2,1333,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/metric_data/Output/Bytes"},[1,1253,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/transaction_sample_data/Output/Bytes"},[1,80,0,0,0,0]]]]`
	if got, _ := OrderScrubMetrics(cp3.data, nil); string(got) != usageMetrics {
		t.Fatal(string(got))
	}

	m.QuitTestProcessor()
}

// split Php Packages out from DefaultData test - trying to
// combine the txn trace and php packages data did not work
// as these goto different endpoints - combining them here
// seemed to combine the data which is incorrect
func TestProcessorHarvestDefaultDataPhpPackages(t *testing.T) {
	m := NewMockedProcessor(2)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnPhpPackagesSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}

	// collect php packages
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp_pkgs := <-m.clientParams
	// collect metrics
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp_metrics := <-m.clientParams
	// collect usage metrics
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp_usage := <-m.clientParams

	<-m.p.trackProgress // unblock processor after harvest

	// check pkgs and metric data - it appears these can
	// come in different orders so check both
	toTestPkgs := `["Jars",["package", "1.2.3",{}]]`
	if toTestPkgs != string(cp_pkgs.data) {
		if toTestPkgs != string(cp_metrics.data) {
			t.Fatalf("packages data: expected '%s', got '%s'", toTestPkgs, string(cp_pkgs.data))
		}
	}

	time1 := strings.Split(string(cp_usage.data), ",")[1]
	time2 := strings.Split(string(cp_usage.data), ",")[2]
	usageMetrics := `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[2,1285,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/metric_data/Output/Bytes"},[1,1253,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/update_loaded_modules/Output/Bytes"},[1,32,0,0,0,0]]]]`
	if got, _ := OrderScrubMetrics(cp_usage.data, nil); string(got) != usageMetrics {
		t.Fatalf("metrics data: expected '%s', got '%s'", string(usageMetrics), string(got))
	}
	m.QuitTestProcessor()
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
	/* collect metrics */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp := <-m.clientParams

	<-m.p.trackProgress // unblock processor after harvest

	expected := `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]`
	if string(cp.data) != expected {
		t.Fatalf("expected: %s \ngot: %s", expected, string(cp.data))
	}

	m.QuitTestProcessor()
}

func TestProcessorHarvestLogEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	appInfo := NewAppInfoWithLogEventLimit(1000)

	m.DoAppInfoCustom(t, nil, AppStateUnknown, &appInfo)

	m.DoConnect(t, &idOne)

	m.DoAppInfoCustom(t, nil, AppStateConnected, &appInfo)

	m.TxnData(t, idOne, txnLogEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestLogEvents,
	}
	/* collect logs */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp := <-m.clientParams

	<-m.p.trackProgress // unblock processor after harvest

	expected := `[{"common": {"attributes": {}},"logs": [log event test birthday]}]`
	if string(cp.data) != expected {
		t.Fatalf("expected: %s \ngot: %s", expected, string(cp.data))
	}

	m.QuitTestProcessor()
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
	m.clientReturn <- ClientReturn{} /* usage metrics */

	m.p.CleanExit()

	<-m.clientParams        /* ditch metrics */
	cp := <-m.clientParams  /* custom events */
	cp2 := <-m.clientParams /* usage metrics */

	expected := `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]`
	if string(cp.data) != expected {
		t.Fatalf("expected: %s \ngot: %s", expected, string(cp.data))
	}

	time1 := strings.Split(string(cp2.data), ",")[1]
	time2 := strings.Split(string(cp2.data), ",")[2]
	usageMetrics := `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[2,1313,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/custom_event_data/Output/Bytes"},[1,60,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/metric_data/Output/Bytes"},[1,1253,0,0,0,0]]]]`
	if got, _ := OrderScrubMetrics(cp2.data, nil); string(got) != usageMetrics {
		t.Fatal(string(got))
	}
}

func TestUsageHarvest(t *testing.T) {
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
	/* collect metrics */
	cp1 := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}
	/* collect usage metrics */
	cp2 := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}

	<-m.p.trackProgress // unblock processor after harvest

	// Because MockedProcessor wraps a real processor, we have no way to directly set the time
	//   of harvests. So we extract the time from what we receive
	time1 := strings.Split(string(cp1.data), ",")[1]
	time2 := strings.Split(string(cp1.data), ",")[2]
	var expectedJSON1 = `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Instance/Reporting"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/AnalyticEventData/HarvestLimit"},[10000,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/CustomEventData/HarvestLimit"},[5,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ErrorEventData/HarvestLimit"},[5,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/LogEventData/HarvestLimit"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[5000000000,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSent"},[0,0,0,0,0,0]]]]`
	time1 = strings.Split(string(cp2.data), ",")[1]
	time2 = strings.Split(string(cp2.data), ",")[2]
	var expectedJSON2 = `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[1,1253,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/metric_data/Output/Bytes"},[1,1253,0,0,0,0]]]]`

	if got1, _ := OrderScrubMetrics(cp1.data, nil); string(got1) != expectedJSON1 {
		t.Errorf("\ngot=%q \nwant=%q", got1, expectedJSON1)
	}
	if got2, _ := OrderScrubMetrics(cp2.data, nil); string(got2) != expectedJSON2 {
		t.Errorf("\ngot=%q \nwant=%q", got2, expectedJSON2)
	}
	m.QuitTestProcessor()
}

func TestUsageHarvestExceedChannel(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	// Harvest enough data that the data usage channel overflows and drops data
	// Make the harvest blocking so that we are guaranteed channel fills before accessing it
	for i := 0; i < 30; i++ {
		m.TxnData(t, idOne, txnEventSample1Times(10))
		m.processorHarvestChan <- ProcessorHarvest{
			AppHarvest: m.p.harvests[idOne],
			ID:         idOne,
			Type:       HarvestTxnEvents,
			Blocking:   true,
		}
		/* collect txn data */
		<-m.clientParams
		m.clientReturn <- ClientReturn{nil, nil, 202}
		<-m.p.trackProgress // unblock processor after harvest
	}

	m.TxnData(t, idOne, txnEventSample1Times(10))
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}
	// Need to have other data, because data usage is not harvested on empty harvest
	/* collect txn data */
	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}
	/* collect usage metrics */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}

	<-m.p.trackProgress // unblock processor after harvest

	time1 := strings.Split(string(cp.data), ",")[1]
	time2 := strings.Split(string(cp.data), ",")[2]
	// The data usage channel only holds 25 points until dropping data
	var expectedJSON = `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[25,5275,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/analytic_event_data/Output/Bytes"},[25,5275,0,0,0,0]]]]`

	if got, _ := OrderScrubMetrics(cp.data, nil); string(got) != expectedJSON {
		t.Errorf("\ngot=%q \nwant=%q", got, expectedJSON)
	}
	m.QuitTestProcessor()
}

func TestSupportabilityHarvest(t *testing.T) {
	m := NewMockedProcessor(1)

	appInfo := NewAppInfoWithLogEventLimit(1000)

	m.DoAppInfoCustom(t, nil, AppStateUnknown, &appInfo)

	m.DoConnect(t, &idOne)

	m.DoAppInfoCustom(t, nil, AppStateConnected, &appInfo)

	m.TxnData(t, idOne, txnErrorEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}

	<-m.p.trackProgress // unblock processor after harvest to receive error

	/* metrics */
	// Add timeout error response code
	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, ErrUnsupportedMedia, 408}
	/* usage metrics */
	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}

	<-m.p.trackProgress // unblock processor after harvest error

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}

	/* error event */
	cp1 := <-m.clientParams
	m.clientReturn <- ClientReturn{}
	/* usage metrics */
	cp2 := <-m.clientParams
	m.clientReturn <- ClientReturn{}

	<-m.p.trackProgress // unblock processor after harvest

	// Because MockedProcessor wraps a real processor, we have no way to directly set the time
	//   of harvests. So we extract the time from what we receive
	time1 := strings.Split(string(cp1.data), ",")[1]
	time2 := strings.Split(string(cp1.data), ",")[2]
	var expectedJSON = `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Instance/Reporting"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/Agent/Collector/HTTPError/408"},[1,0,0,0,0,0]],` + // Check for HTTPError Supportability metric
		`[{"name":"Supportability/Agent/Collector/metric_data/Attempts"},[1,0,0,0,0,0]],` + //	Metrics were sent first when the 408 error occurred, so check for the metric failure.
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/AnalyticEventData/HarvestLimit"},[20000,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/CustomEventData/HarvestLimit"},[10,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ErrorEventData/HarvestLimit"},[10,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/LogEventData/HarvestLimit"},[10,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/ReportPeriod"},[10000000000,0,0,0,0,0]],` +
		`[{"name":"Supportability/EventHarvest/SpanEventData/HarvestLimit"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[2,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Logging/Forwarding/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/SpanEvent/TotalEventsSent"},[0,0,0,0,0,0]]]]`
	time1 = strings.Split(string(cp2.data), ",")[1]
	time2 = strings.Split(string(cp2.data), ",")[2]
	// includes usage of the first data usage metrics sent
	var expectedJSON2 = `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[2,1584,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/metric_data/Output/Bytes"},[2,1584,0,0,0,0]]]]`

	if got, _ := OrderScrubMetrics(cp1.data, nil); string(got) != expectedJSON {
		t.Errorf("\ngot=%q \nwant=%q", got, expectedJSON)
	}
	if got2, _ := OrderScrubMetrics(cp2.data, nil); string(got2) != expectedJSON2 {
		t.Errorf("\ngot=%q \nwant=%q", got2, expectedJSON2)
	}
	m.QuitTestProcessor()
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

	<-m.p.trackProgress // unblock processor after harvest
	/* error events */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}

	if string(cp.data) != `["one",{"reservoir_size":5,"events_seen":1},[forgotten birthday]]` {
		t.Fatal(string(cp.data))
	}

	m.QuitTestProcessor()
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

	<-m.p.trackProgress // unblock processor after harvest
	/* span events */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil, 202}

	if string(cp.data) != `["one",{"reservoir_size":7,"events_seen":2},[belated birthday,belated birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.QuitTestProcessor()
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
	<-m.p.trackProgress // unblock processor after harvest

	// Now we'll force a harvest for a different event type, and make sure we
	// receive that harvest (and not a span event harvest).
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestCustomEvents,
	}
	<-m.p.trackProgress // unblock processor after harvest

	/* custom events */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp := <-m.clientParams

	if string(cp.data) != `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.QuitTestProcessor()
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

	<-m.p.trackProgress // unblock processor after harvest, with 2 span events seen, but only one span sent

	/* span event */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp := <-m.clientParams

	if string(cp.data) != `["one",{"reservoir_size":1,"events_seen":2},[belated birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.QuitTestProcessor()
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
	<-m.p.trackProgress // unblock processor

	// Now we'll force a harvest for a different event type, and make sure we
	// receive that harvest (and not an error event harvest).
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestCustomEvents,
	}
	<-m.p.trackProgress // unblock processor

	/* custom events */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp := <-m.clientParams

	if string(cp.data) != `["one",{"reservoir_size":5,"events_seen":1},[half birthday]]` {
		t.Fatal(string(cp.data))
	}
	m.QuitTestProcessor()
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

	<-m.p.trackProgress // unblock processor
	/* txn events */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp1 := <-m.clientParams

	cp1Events := getEventsSeen(cp1.data)
	if cp1Events != 9000 {
		t.Fatal("Expected 9000 events")
	}

	m.QuitTestProcessor()

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
		// blocking to get data usage correct
		Blocking: true,
	}
	/* txn events */
	cp1 = <-m.clientParams
	m.clientReturn <- ClientReturn{}
	<-m.p.trackProgress // unblock processor

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
		// blocking to get data usage correct
		Blocking: true,
	}
	/* txn events first payload */
	cp1 = <-m.clientParams
	m.clientReturn <- ClientReturn{}
	/* txn events second payload */
	cp2 := <-m.clientParams
	m.clientReturn <- ClientReturn{}

	<-m.p.trackProgress // unblock processor

	cp1Events = getEventsSeen(cp1.data)
	cp2Events := getEventsSeen(cp2.data)
	if cp1Events != 2500 {
		t.Fatal("Payload with 2500 events expected, got ", cp1Events)
	}
	if cp2Events != 2500 {
		t.Fatal("Payload with 2500 events expected, got ", cp2Events)
	}

	// 8001 events. Split into two payloads of 4000 and 4001.
	// Test that data usage metrics count properly in this case
	m.TxnData(t, idOne, txnEventSample1Times(8001))
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		// harvest both txn events and metrics
		Type: HarvestTxnEvents | HarvestDefaultData,
		// needs to be blocking to know order of clientParams
		Blocking: true,
	}
	/* metrics */
	<-m.clientParams
	m.clientReturn <- ClientReturn{}
	/* txn events first payload */
	cp1 = <-m.clientParams
	m.clientReturn <- ClientReturn{}
	/* txn events second payload */
	cp2 = <-m.clientParams
	m.clientReturn <- ClientReturn{}
	/* usage metrics */
	cp3 := <-m.clientParams
	m.clientReturn <- ClientReturn{}

	<-m.p.trackProgress // unblock processor

	// txn events comparison
	cp1Events = getEventsSeen(cp1.data)
	cp2Events = getEventsSeen(cp2.data)
	if cp1Events != 4000 && cp2Events != 4000 {
		t.Fatal("Payloads with 4000 events expected, got ", cp1Events, " and ", cp2Events)
	}
	if (cp1Events + cp2Events) != 8001 {
		t.Fatal("Payload sum of 8001 events expected, got ", cp1Events, " and ", cp2Events)
	}
	// usage metrics comparison
	time1 := strings.Split(string(cp3.data), ",")[1]
	time2 := strings.Split(string(cp3.data), ",")[2]
	var expectedJSON = `["one",` + time1 + `,` + time2 + `,` +
		`[[{"name":"Supportability/C/Collector/Output/Bytes"},[6,289520,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/analytic_event_data/Output/Bytes"},[5,288261,0,0,0,0]],` +
		`[{"name":"Supportability/C/Collector/metric_data/Output/Bytes"},[1,1259,0,0,0,0]]]]`

	if got, _ := OrderScrubMetrics(cp3.data, nil); string(got) != expectedJSON {
		t.Errorf("\ngot=%q \nwant=%q", got, expectedJSON)
	}

	m.QuitTestProcessor()
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
	<-m.p.trackProgress // unblock processor

	// Test processor receiving restart exception
	/* txn events */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleRestartException, 401}
	<-m.p.trackProgress // unblock processor after handling harvest restart error

	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	// Reconnect after restart exception
	m.DoConnect(t, &idTwo)
	m.DoAppInfo(t, &idOne, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)
	m.TxnData(t, idTwo, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idTwo],
		ID:         idTwo,
		Type:       HarvestTxnEvents,
	}

	<-m.p.trackProgress // unblock processor after  harvest notice
	/* txn events */
	m.clientReturn <- ClientReturn{nil, nil, 202}
	cp = <-m.clientParams

	if string(cp.data) != `["two",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.QuitTestProcessor()
}

func TestDisconnectAtPreconnect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.QuitTestProcessor()
}

func TestLicenseExceptionAtPreconnect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{nil, SampleLicenseInvalidException, 401}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateInvalidLicense)

	m.QuitTestProcessor()
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

	m.QuitTestProcessor()
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
	<-m.p.trackProgress // unblock after harvest notice

	/* txn data */
	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // unblock after harvest error

	/* metrics */
	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // unblock after harvest error

	/* usage metrics */
	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleDisonnectException, 410}
	<-m.p.trackProgress // unblock after harvest error

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.QuitTestProcessor()
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
	<-m.p.trackProgress // unblock after harvest notice

	/* txn */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, SampleLicenseInvalidException, 401}
	<-m.p.trackProgress // unblock after harvest error

	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	// Unknown app state triggered immediately following AppStateRestart
	m.DoAppInfo(t, nil, AppStateUnknown)

	m.QuitTestProcessor()
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

	m.QuitTestProcessor()
}

func TestMalformedCollector(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // preconnect
	m.clientReturn <- ClientReturn{[]byte(`"`), nil, 200}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.QuitTestProcessor()
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
	<-m.p.trackProgress // unblock after harvest notice

	/* txn events */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, errors.New("unusual error"), 500}
	<-m.p.trackProgress // unblock after harvest error

	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // unblock after harvest notice

	/* txn events */
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
	<-m.p.trackProgress // unblock after harvest notice

	/* txn events */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, ErrPayloadTooLarge, 413}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // unblock after harvest error

	m.TxnData(t, idOne, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // unblock after harvest notice

	/* txn events */
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
	<-m.p.trackProgress // unblock after harvest notice

	/* txn events */
	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, ErrUnsupportedMedia, 415}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // unblock after harvest error

	m.TxnData(t, idOne, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // unblock after harvest notice

	/* txn events */
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
	connectClient = collector.ClientFn(func(cmd *collector.RpmCmd, cs collector.RpmControls) collector.RPMResponse {
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

// runs a mocked test of resolution of agent harvest limit request and value returned by collector
// Notes:
//   eventType:     "log_event_data" or "custom_event_data" (no others supported currently)
//   agentLimit:     Harvest limit from agent (INI file) for a 60 second harvest period
//   collectorLimit: Harvest limit sent from collector for a 5 second harvest period
//
// Be aware the agentLimit will be scaled down by 12 (60/5) before being compared to the
// collectorLimit.
func runMockedCollectorHarvestLimitTest(t *testing.T, eventType string, agentLimit uint64, collectorLimit uint64, testName string) {

	// setup non-zero agent limits for log events
	// NOTE: This limit is based on a 60 second harvest period!
	//       So the actual value used to compare to the collector
	//       limit will be 1/12th (5s/60s) smaller
	var appInfo AppInfo = AppInfo{}

	// defaults that collector will send if no harvest limit requested in connect request
	logHarvestLimit := 833
	customHarvestLimit := 2500

	switch eventType {
	case "log_event_data":
		logHarvestLimit = int(collectorLimit)
		appInfo = NewAppInfoWithLogEventLimit(int(agentLimit))
	case "custom_event_data":
		customHarvestLimit = int(collectorLimit)
		appInfo = NewAppInfoWithCustomEventLimit(int(agentLimit))
	default:
		t.Fatalf("%s: runMockedCollectorHarvestLimitTest() invalid eventType \"%s\" specified", testName, eventType)
	}

	m := NewMockedProcessor(1)

	id := AgentRunID("log")

	reply := m.p.IncomingAppInfo(nil, &appInfo)
	<-m.p.trackProgress // receive app info
	if reply.State != AppStateUnknown {
		t.Fatal("\"", testName, "\": expected ", AppStateUnknown, " got ", reply.State)
	}

	// mock collector response specifying non-zero log harvest limit
	mockedReply := `{"agent_run_id":"` + id.String() +
		`","zip":"zap",` +
		`"span_event_harvest_config":{"report_period_ms":5000,"harvest_limit":166},` +
		`"event_harvest_config":{"report_period_ms":5000,` +
		`"harvest_limits":{"analytics_event_data":833, "error_event_data":833,` +
		`"custom_event_data":` + strconv.Itoa(customHarvestLimit) + `,` +
		`"log_event_data":` + strconv.Itoa(logHarvestLimit) + `}}}`
	m.DoConnectConfiguredReply(t, mockedReply)

	reply = m.p.IncomingAppInfo(nil, &appInfo)
	<-m.p.trackProgress // receive app info
	if reply.State != AppStateConnected {
		t.Fatal("\"", testName, "\": expected ", AppStateConnected, " got ", reply.State)
	}

	// should be 1 app in processor map and name should match that in appinfo
	if 1 != len(m.p.apps) {
		t.Error("\"", testName, "\": Exactly one application should be created.")
	}

	app, found := m.p.apps[appInfo.Key()]
	if !found {
		t.Error("\"", testName, "\":Expected application not found in processor map.")
	}

	// verify the client value was choosen
	scaledAgentLimit := uint64(agentLimit / 12)
	expectedLimit := uint64(collectorLimit)
	if scaledAgentLimit < collectorLimit {
		expectedLimit = scaledAgentLimit
	}
	finalLimit := -1
	switch eventType {
	case "log_event_data":
		finalLimit = app.connectReply.EventHarvestConfig.EventConfigs.LogEventConfig.Limit
	case "custom_event_data":
		finalLimit = app.connectReply.EventHarvestConfig.EventConfigs.CustomEventConfig.Limit
	default:
		t.Fatalf("%s: runMockedCollectorHarvestLimitTest() invalid eventType \"%s\" specified", testName, eventType)
	}
	if int(expectedLimit) != finalLimit {
		t.Errorf("\" %s \": Expected %d to be choosen but %d was instead", testName, expectedLimit, finalLimit)
	}

	m.p.quit()
}

func TestConnectNegotiateLogEventLimits(t *testing.T) {

	// these tests exist for the log events (TestConnectNegotiateLogEventsLimit()) because at some point
	// the collector would return the default limit (833) if 0 was passed as the log limit.  These tests
	// exercise logic in the daemon to enforce the smaller agent value
	runMockedCollectorHarvestLimitTest(t, "log_event_data", 90*12, 100, "agent log limit smaller than collector")
	runMockedCollectorHarvestLimitTest(t, "log_event_data", 110*12, 100, "agent log limit larger than collector")
	runMockedCollectorHarvestLimitTest(t, "log_event_data", 100*12, 100, "agent log limit equal to collector")
	runMockedCollectorHarvestLimitTest(t, "log_event_data", 0, 100, "agent log limit == 0, collector != 0")
	runMockedCollectorHarvestLimitTest(t, "log_event_data", 100*12, 0, "agent log limit != 0, collector == 0")

}

func TestConnectNegotiateCustomEventLimits(t *testing.T) {

	runMockedCollectorHarvestLimitTest(t, "custom_event_data", 110*12, 100, "agent custom limit larger than collector")
	runMockedCollectorHarvestLimitTest(t, "custom_event_data", 100*12, 100, "agent custom limit equal to collector")
}

func TestProcessLogEventLimit(t *testing.T) {
	// nil as argument should just return
	processLogEventLimits(nil)

	// empty App should just return (no info or connectReply)
	emptyApp := App{}
	processLogEventLimits(&emptyApp)

	// only info defined should just return (no connectReply)
	infoOnlyApp := App{info: &AppInfo{}}
	processLogEventLimits(&infoOnlyApp)

	// only connectReply defined should just return (no info)
	connectReplyOnlyApp := App{connectReply: &ConnectReply{}}
	processLogEventLimits(&connectReplyOnlyApp)
}

func TestMissingAgentAndCollectorHarvestLimit(t *testing.T) {

	// tests missing agent and collector harvest limits will not crash daemon
	// and results in empty final harvest values
	appInfo := sampleAppInfo

	m := NewMockedProcessor(1)

	id := AgentRunID("log")

	reply := m.p.IncomingAppInfo(nil, &appInfo)
	<-m.p.trackProgress // receive app info
	if reply.State != AppStateUnknown {
		t.Fatal("Expected ", AppStateUnknown, " got ", reply.State)
	}

	// mock collector response specifying no harvest limit and test
	// code handles it gravefully
	mockedReply := `{"agent_run_id":"` + id.String() + `","zip":"zap"}`
	m.DoConnectConfiguredReply(t, mockedReply)

	reply = m.p.IncomingAppInfo(nil, &appInfo)
	<-m.p.trackProgress // receive app info
	if reply.State != AppStateConnected {
		t.Fatal("Expected ", AppStateConnected, " got ", reply.State)
	}

	// should be 1 app in processor map and name should match that in appinfo
	if 1 != len(m.p.apps) {
		t.Error("Exactly one application should be created.")
	}

	app, found := m.p.apps[appInfo.Key()]
	if !found {
		t.Error("Expected application not found in processor map.")
	}

	expected := collector.EventHarvestConfig{}

	if expected != app.connectReply.EventHarvestConfig {
		actualStr, _ := json.MarshalIndent(app.connectReply.EventHarvestConfig, "", "\t")
		expectedStr, _ := json.MarshalIndent(expected, "", "\t")
		t.Errorf("Expected %s got %s", string(expectedStr), string(actualStr))
	}

	m.p.quit()
}
