//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package flatbuffersdata

import (
	"reflect"
	"testing"
	"time"

	"newrelic.com/daemon/newrelic"
	"newrelic.com/daemon/newrelic/collector"
	"newrelic.com/daemon/newrelic/limits"
	"newrelic.com/daemon/newrelic/protocol"
)

func BenchmarkAggregateTxn(b *testing.B) {
	data, err := SampleTxn.MarshalBinary()
	if nil != err {
		b.Fatal(err)
	}

	ag := newrelic.FlatTxn(data)
	harvest := newrelic.NewHarvest(time.Now(), collector.NewHarvestLimits(nil))

	// Add the metrics, so we are only doing lookups in the loop
	ag.AggregateInto(harvest)

	b.ReportAllocs()
	b.ResetTimer() // reset memory allocation counters

	for i := 0; i < b.N; i++ {
		ag.AggregateInto(harvest)
	}
}

func TestFlatbuffersAppInfo(t *testing.T) {
	data, err := MarshalAppInfo(&SampleAppInfo)
	if nil != err {
		t.Fatal(err)
	}

	msg := protocol.GetRootAsMessage(data, 0)
	if msg.DataType() != protocol.MessageBodyApp {
		t.Fatal(msg.DataType())
	}

	var tbl flatbuffers.Table
	if !msg.Data(&tbl) {
		t.Fatal("missing data")
	}

	out := newrelic.UnmarshalAppInfo(tbl)
	if nil == out {
		t.Fatal(out)
	}

	if out.License != SampleAppInfo.License {
		t.Fatal(out.License, SampleAppInfo.License)
	}
	if out.Appname != SampleAppInfo.Appname {
		t.Fatal(out.Appname, SampleAppInfo.Appname)
	}
	if out.AgentLanguage != SampleAppInfo.AgentLanguage {
		t.Fatal(out.AgentLanguage, SampleAppInfo.AgentLanguage)
	}
	if out.AgentVersion != SampleAppInfo.AgentVersion {
		t.Fatal(out.AgentVersion, SampleAppInfo.AgentVersion)
	}
	if !reflect.DeepEqual(out.Settings, SampleAppInfo.Settings) {
		t.Fatal(out.Settings, SampleAppInfo.Settings)
	}
	if string(out.Environment) != string(SampleAppInfo.Environment) {
		t.Fatal(string(out.Environment), string(SampleAppInfo.Environment))
	}
	if out.HighSecurity != SampleAppInfo.HighSecurity {
		t.Fatal(out.HighSecurity, SampleAppInfo.HighSecurity)
	}
	if string(out.Labels) != string(SampleAppInfo.Labels) {
		t.Fatal(string(out.Labels), string(SampleAppInfo.Labels))
	}
	if out.RedirectCollector != SampleAppInfo.RedirectCollector {
		t.Fatal(out.RedirectCollector, SampleAppInfo.RedirectCollector)
	}
	if out.TraceObserverHost != SampleAppInfo.TraceObserverHost {
		t.Fatal(out.TraceObserverHost, SampleAppInfo.TraceObserverHost)
	}
	if out.TraceObserverPort != SampleAppInfo.TraceObserverPort {
		t.Fatal(out.TraceObserverPort, SampleAppInfo.TraceObserverPort)
	}
}

func TestFlatbuffersTxnData(t *testing.T) {
	txn := Txn{
		Name:             "heyo",
		SamplingPriority: 0.80000,
		Metrics: []metric{
			metric{Name: "scoped", Data: [6]float64{1, 2, 3, 4, 5, 6}, Scoped: true},
			metric{Name: "forced", Data: [6]float64{6, 5, 4, 3, 2, 1}, Forced: true},
		},
		Errors: []*newrelic.Error{SampleError},
		Trace: &newrelic.TxnTrace{
			MetricName:           "heyo",
			RequestURI:           "alpha/beta/gamma",
			UnixTimestampMillis:  123456.123456,
			DurationMillis:       2001,
			Data:                 newrelic.JSONString("[]"),
			GUID:                 "abcdef0123456789",
			ForcePersist:         false,
			SyntheticsResourceID: "1234",
		},
		SlowSQLs: []*newrelic.SlowSQL{
			&newrelic.SlowSQL{
				MetricName:  "insert",
				ID:          newrelic.SQLId(1),
				Count:       20,
				TotalMicros: 1000,
				MinMicros:   25,
				MaxMicros:   75,
				Params:      newrelic.JSONString(`{"backtrace":["zip", "zap"]}`),
				Query:       "select * from heyo",
				TxnName:     "heyo",
				TxnURL:      "zip/zap",
			},
		},
		AnalyticEvent: SampleAnalyticEvent,
		CustomEvents:  SampleCustomEvents,
		ErrorEvents:   SampleErrorEvents,
		SpanEvents:    SampleSpanEvents,
	}

	data, err := txn.MarshalBinary()
	if nil != err {
		t.Fatal(err)
	}
	harvest := newrelic.NewHarvest(time.Now(), collector.NewHarvestLimits(nil))
	ag := newrelic.FlatTxn(data)
	ag.AggregateInto(harvest)
	id := newrelic.AgentRunID("12345")

	var out []byte

	s := harvest.Metrics.DebugJSON()
	// We pull the expected JSON in from an external file because Go 1.8 slightly
	// changed how numbers are encoded, and that allows us to use conditional
	// compilation to handle this rather than the flakier runtime.Version()
	// string.
	expect := testFlatbuffersTxnDataExpectedJSON
	if s != expect {
		t.Fatal(s, expect)
	}

	now := time.Now()

	out, err = harvest.Errors.Data(id, now)
	expected := `["12345",[` + string(SampleError.Data) + `]]`
	if string(out) != expected {
		t.Fatal(string(out), expected)
	}

	out, err = harvest.SlowSQLs.Audit(id, now)
	if nil != err || string(out) != `[[["heyo","alpha/beta/gamma",1,"select * from heyo","insert",`+
		`20,1,0.025,0.075,{"backtrace":["zip","zap"]}]]]` {
		t.Fatal(err, string(out))
	}

	out, err = harvest.TxnTraces.Audit(id, now)
	if nil != err || string(out) != `["12345",[[123456.123456,2001,"heyo","alpha/beta/gamma",[],"abcdef0123456789",null,false,null,"1234"]]]` {
		t.Fatal(err, string(out))
	}

	out, err = harvest.TxnEvents.Data(id, now)
	if nil != err || string(out) != `["12345",{"reservoir_size":10000,"events_seen":1},[`+string(SampleAnalyticEvent)+`]]` {
		t.Fatal(err, string(out))
	}

	out, err = harvest.CustomEvents.Data(id, now)
	if nil != err || string(out) != `["12345",{"reservoir_size":100000,"events_seen":3},[[{"x":1}],[{"x":2}],[{"x":3}]]]` {
		t.Fatal(err, string(out))
	}

	out, err = harvest.SpanEvents.Data(id, now)
	if nil != err || string(out) != `["12345",{"reservoir_size":10000,"events_seen":3},[[{"Span1":1}],[{"Span2":2}],[{"Span3":3}]]]` {
		t.Fatal(err, string(out))
	}

	out, err = harvest.ErrorEvents.Data(id, now)
	expected = `["12345",{"reservoir_size":100,"events_seen":1},[[{"type": "TransactionError",` +
		`"timestamp": 1445290225.1948,"error.class": "HeyoException",` +
		`"error.message": "Uncaught exception 'HeyoException' with message 'foo!' in /Users/earnold/workspace/php_integration_tests/integration/errors/heyo.php:6",` +
		`"transactionName": "OtherTransaction/php/heyo.php","duration": 0.00101,"nr.transactionGuid": "390b8adab3b435c2"}]]]`
	if nil != err || string(out) != expected {
		t.Fatal(err, string(out))
	}

}

func TestMinimumFlatbufferSize(t *testing.T) {
	buf := flatbuffers.NewBuilder(0)
	protocol.MessageStart(buf)
	buf.Finish(protocol.MessageEnd(buf))

	minLen := len(buf.Bytes[buf.Head():])
	if minLen != limits.MinFlatbufferSize {
		t.Fatalf("Unexpected minimum flatbuffer size: %d", minLen)
	}
}
