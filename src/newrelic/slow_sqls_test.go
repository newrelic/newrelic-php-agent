//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import "testing"

func sampleSlowSQL(id SQLId) *SlowSQL {
	return &SlowSQL{
		MetricName:  "metric_name",
		Count:       4,
		TotalMicros: 10 * 1000,
		MinMicros:   2 * 1000,
		MaxMicros:   3 * 1000,
		Params:      JSONString(`{"x":1}`),
		Query:       "SELECT *",
		TxnName:     "txn_name",
		TxnURL:      "txn_url",
		ID:          id,
	}
}

func TestSlowSQLs(t *testing.T) {
	slows := NewSlowSQLs(10)
	slows.Observe(sampleSlowSQL(123))

	var json JSONString
	var err error
	json, err = slows.CollectorJSON(false)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != "[[[\"txn_name\",\"txn_url\",123,\"SELECT *\",\"metric_name\",4,10,2,3,{\"x\":1}]]]" {
		t.Error(string(json))
	}
	json, err = slows.CollectorJSON(true)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != "[[[\"txn_name\",\"txn_url\",123,\"SELECT *\",\"metric_name\",4,10,2,3,\"eJyqVqpQsjKsBQQAAP//CJ0CIA==\"]]]" {
		t.Error(string(json))
	}
}

func TestEmptySlowSQLs(t *testing.T) {
	slows := NewSlowSQLs(10)
	var err error
	var json JSONString
	json, err = slows.CollectorJSON(false)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `[[]]` {
		t.Error(string(json))
	}
	json, err = slows.CollectorJSON(true)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `[[]]` {
		t.Error(string(json))
	}
}

func TestMerge(t *testing.T) {
	slows := NewSlowSQLs(1)
	sampleA := sampleSlowSQL(123)
	sampleB := sampleSlowSQL(123)

	// we need to calculate this up front, since the
	// merge will mutate the samples
	expectedCount := sampleA.Count + sampleB.Count

	slows.Observe(sampleA)
	slows.Observe(sampleB)

	if len(slows.slowSQLs) != 1 {
		t.Error("Slows array grew in size")
	}

	if slows.slowSQLs[0].Count != expectedCount {
		t.Errorf("Expected count of merged SQL to be %d, not %d",
			expectedCount, slows.slowSQLs[0].Count)
	}
}

func TestFastFirst(t *testing.T) {
	slows := NewSlowSQLs(1)
	fastSample := sampleSlowSQL(123)
	slowSample := sampleSlowSQL(124)
	slowSample.MaxMicros = fastSample.MaxMicros + 1000

	slows.Observe(fastSample)
	slows.Observe(slowSample)

	if len(slows.slowSQLs) != 1 {
		t.Error("Slows array grew in size")
	}

	if slows.slowSQLs[0].MaxMicros != slowSample.MaxMicros {
		t.Error("fastSample should have been overwritten by slowSample")
	}
}

func TestSlowFirst(t *testing.T) {
	slows := NewSlowSQLs(1)
	fastSample := sampleSlowSQL(123)
	slowSample := sampleSlowSQL(124)
	slowSample.MaxMicros = fastSample.MaxMicros + 1000

	slows.Observe(slowSample)
	slows.Observe(fastSample)

	if len(slows.slowSQLs) != 1 {
		t.Error("Slows array grew in size")
	}

	if slows.slowSQLs[0].MaxMicros != slowSample.MaxMicros {
		t.Error("slowSample should not have been overwritten by fastSample")
	}
}

func TestSlowSqlsEmpty(t *testing.T) {
	slows := NewSlowSQLs(10)
	if !slows.Empty() {
		t.Fatal("slow sqls should be empty")
	}
	slows.Observe(sampleSlowSQL(124))
	if slows.Empty() {
		t.Fatal("slow sqls should not be empty")
	}
}
