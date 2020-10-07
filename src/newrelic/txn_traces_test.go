//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import "testing"

func sampleTxnTrace() *TxnTrace {
	return &TxnTrace{
		MetricName:           "my_name",
		RequestURI:           "my_uri",
		UnixTimestampMillis:  123456.123465,
		DurationMillis:       2000.00,
		Data:                 JSONString(`{"x":1}`),
		GUID:                 "078ad44c1960eab7",
		ForcePersist:         true,
		SyntheticsResourceID: "",
	}
}

func TestTxnTrace(t *testing.T) {
	trace := sampleTxnTrace()
	traces := NewTxnTraces()
	traces.AddTxnTrace(trace)

	id := AgentRunID(`12345`)
	json, err := traces.CollectorJSON(id, false)
	if nil != err {
		t.Fatal(err)
	}
	expectedJSON := `["12345",[[123456.123465,2000,"my_name","my_uri",{"x":1},"078ad44c1960eab7",null,true,null,null]]]`

	if string(json) != expectedJSON {
		t.Error(string(json))
	}
}

func TestEmptyTraces(t *testing.T) {
	traces := NewTxnTraces()
	id := AgentRunID(`12345`)
	json, err := traces.CollectorJSON(id, false)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",[]]` {
		t.Error(string(json))
	}
}
