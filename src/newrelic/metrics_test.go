//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"strconv"
	"testing"
	"time"

	"newrelic/limits"
)

var start = time.Date(2014, time.November, 28, 1, 1, 0, 0, time.UTC)
var end = time.Date(2014, time.November, 28, 1, 2, 0, 0, time.UTC)

func addDuration(mt *MetricTable, name, scope string, duration, exclusive time.Duration, force MetricForce) {
	data := metricData{
		countSatisfied:  1,
		totalTolerated:  duration.Seconds(),
		exclusiveFailed: exclusive.Seconds(),
		min:             duration.Seconds(),
		max:             duration.Seconds(),
		sumSquares:      duration.Seconds() * duration.Seconds(),
	}
	mt.add(nil, name, scope, data, force)
}

func TestEmptyMetrics(t *testing.T) {
	mt := NewMetricTable(20, start)
	js, err := mt.CollectorJSON(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if want := `["12345",1417136460,1417136520,[]]`; string(js) != want {
		t.Errorf("got=%q want=%q", js, want)
	}
}

func TestMetrics(t *testing.T) {
	mt := NewMetricTable(20, start)

	addDuration(mt, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(mt, "two", "my_scope", 4*time.Second, 2*time.Second, Unforced)
	addDuration(mt, "one", "my_scope", 2*time.Second, 1*time.Second, Unforced)
	addDuration(mt, "one", "", 2*time.Second, 1*time.Second, Unforced)

	var expectedJSON = `["12345",1417136460,1417136520,[` +
		`[{"name":"one"},[2,4,2,2,2,8]],` +
		`[{"name":"one","scope":"my_scope"},[1,2,1,2,2,4]],` +
		`[{"name":"two","scope":"my_scope"},[1,4,2,4,4,16]]]]`

	json, err := mt.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if got := string(json); got != expectedJSON {
		t.Errorf("\ngot=%s\nwant=%s", got, expectedJSON)
	}
}

func TestApplyRules(t *testing.T) {
	js := `[{"ignore":false,"each_segment":false,"terminate_chain":true,"replacement":"been_renamed","replace_all":false,"match_expression":"one$","eval_order":1}]`
	rules := NewMetricRulesFromJSON([]byte(js))

	mt := NewMetricTable(20, start)
	addDuration(mt, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(mt, "one", "my_scope", 2*time.Second, 1*time.Second, Unforced)

	applied := mt.ApplyRules(rules)
	json, err := applied.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}

	expected := `["12345",1417136460,1417136520,[[{"name":"been_renamed"},[1,2,1,2,2,4]],` +
		`[{"name":"been_renamed","scope":"my_scope"},[1,2,1,2,2,4]]]]`

	if string(json) != expected {
		t.Fatal(string(json))
	}
}

func TestForced(t *testing.T) {
	mt := NewMetricTable(0, start)

	if mt.numDropped != 0 {
		t.Fatal(mt.numDropped)
	}

	addDuration(mt, "unforced", "", 1*time.Second, 1*time.Second, Unforced)
	addDuration(mt, "forced", "", 2*time.Second, 2*time.Second, Forced)

	json, err := mt.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	expected := `["12345",1417136460,1417136520,` +
		`[[{"name":"forced"},[1,2,2,2,2,4]]]]`

	if mt.numDropped != 1 {
		t.Fatal(mt.numDropped)
	}

	if string(json) != expected {
		t.Fatal(string(json))
	}
}

func TestMetricsMergeIntoEmpty(t *testing.T) {
	src := NewMetricTable(20, start)
	addDuration(src, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "two", "", 2*time.Second, 1*time.Second, Unforced)
	dest := NewMetricTable(20, start)
	dest.Merge(src)

	json, err := dest.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	expected := `["12345",1417136460,1417136520,` +
		`[[{"name":"one"},[1,2,1,2,2,4]],` +
		`[{"name":"two"},[1,2,1,2,2,4]]]]`
	if string(json) != expected {
		t.Fatal(string(json))
	}
}

func TestMetricsMergeFromEmpty(t *testing.T) {
	src := NewMetricTable(20, start)
	dest := NewMetricTable(20, start)
	addDuration(dest, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(dest, "two", "", 2*time.Second, 1*time.Second, Unforced)
	dest.Merge(src)

	json, err := dest.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	expected := `["12345",1417136460,1417136520,` +
		`[[{"name":"one"},[1,2,1,2,2,4]],` +
		`[{"name":"two"},[1,2,1,2,2,4]]]]`
	if string(json) != expected {
		t.Fatal(string(json))
	}
}

func TestMetricsMerge(t *testing.T) {
	src := NewMetricTable(20, start)
	dest := NewMetricTable(20, start)
	addDuration(dest, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(dest, "two", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "two", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "three", "", 2*time.Second, 1*time.Second, Unforced)

	dest.Merge(src)

	json, err := dest.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	expected := `["12345",1417136460,1417136520,` +
		`[[{"name":"one"},[1,2,1,2,2,4]],` +
		`[{"name":"three"},[1,2,1,2,2,4]],` +
		`[{"name":"two"},[2,4,2,2,2,8]]]]`
	if string(json) != expected {
		t.Fatal(string(json))
	}
}

func TestMergeFailedSuccess(t *testing.T) {
	src := NewMetricTable(20, start)
	dest := NewMetricTable(20, end)
	addDuration(dest, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(dest, "two", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "two", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "three", "", 2*time.Second, 1*time.Second, Unforced)

	if 0 != dest.failedHarvests {
		t.Fatal(dest.failedHarvests)
	}

	dest.MergeFailed(src)

	json, err := dest.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	expected := `["12345",1417136460,1417136520,` +
		`[[{"name":"one"},[1,2,1,2,2,4]],` +
		`[{"name":"three"},[1,2,1,2,2,4]],` +
		`[{"name":"two"},[2,4,2,2,2,8]]]]`
	if string(json) != expected {
		t.Fatal(string(json))
	}
	if start != dest.metricPeriodStart {
		t.Fatal(dest.metricPeriodStart)
	}
	if 1 != dest.failedHarvests {
		t.Fatal(dest.failedHarvests)
	}
}

func TestMergeFailedLimitReached(t *testing.T) {
	src := NewMetricTable(20, start)
	dest := NewMetricTable(20, end)
	addDuration(dest, "one", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(dest, "two", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "two", "", 2*time.Second, 1*time.Second, Unforced)
	addDuration(src, "three", "", 2*time.Second, 1*time.Second, Unforced)

	src.failedHarvests = limits.FailedMetricAttemptsLimit

	dest.MergeFailed(src)

	json, err := dest.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	expected := `["12345",1417136520,1417136520,` +
		`[[{"name":"one"},[1,2,1,2,2,4]],` +
		`[{"name":"two"},[1,2,1,2,2,4]]]]`
	if string(json) != expected {
		t.Fatal(string(json))
	}
	if end != dest.metricPeriodStart {
		t.Fatal(dest.metricPeriodStart)
	}
	if 0 != dest.failedHarvests {
		t.Fatal(dest.failedHarvests)
	}
}

func TestAddRaw(t *testing.T) {
	mt := NewMetricTable(20, start)

	mt.AddRaw(nil, "one", "", [6]float64{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, Unforced)
	mt.AddRaw([]byte("one"), "", "", [6]float64{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, Unforced)

	mt.AddRaw([]byte("two"), "", "", [6]float64{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, Unforced)
	mt.AddRaw(nil, "two", "", [6]float64{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, Unforced)

	var expectedJSON = `["12345",1417136460,1417136520,` +
		`[[{"name":"one"},[2,4,6,4,5,12]],` +
		`[{"name":"two"},[2,4,6,4,5,12]]]]`
	json, err := mt.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != expectedJSON {
		t.Error(string(json))
	}
}

func BenchmarkMetricTableCollectorJSON(b *testing.B) {
	// Sample metrics derived from MyBlog2 test application in php_test_tools.
	// See the flatbuffersdata package for a more complete example.
	mt := NewMetricTable(2000, time.Now())
	mt.AddRaw(nil, "Apdex", "", [6]float64{332, 26, 3, 0.5, 0.5, 0}, Forced)
	mt.AddRaw(nil, "CPU/User Time", "", [6]float64{361, 2.9386199999999874, 0, 0, 0.06499, 0.14663000000000007}, Forced)
	mt.AddRaw(nil, "CPU/User/Utilization", "", [6]float64{361, 62.965800000000016, 0, 0, 1.28535, 54.57723999999999}, Forced)
	mt.AddRaw(nil, "Datastore/MySQL/all", "", [6]float64{56260, 26.704100000000004, 26.704100000000004, 2e-05, 0.22667, 0.99884}, Forced)
	mt.AddRaw(nil, "Datastore/MySQL/allWeb", "", [6]float64{56260, 26.704100000000004, 26.704100000000004, 2e-05, 0.22667, 0.99884}, Forced)
	mt.AddRaw(nil, "Datastore/all", "", [6]float64{56260, 26.704100000000004, 26.704100000000004, 2e-05, 0.22667, 0.99884}, Forced)
	mt.AddRaw(nil, "Datastore/allWeb", "", [6]float64{56260, 26.704100000000004, 26.704100000000004, 2e-05, 0.22667, 0.99884}, Forced)
	mt.AddRaw(nil, "Datastore/operation/MySQL/insert", "", [6]float64{53000, 23.411430000000006, 23.411430000000006, 4e-05, 0.22667, 0.6342200000000001}, Forced)

	for i := 0; i < 20; i++ {
		scope := "WebTransaction/Uri/myblog2/" + strconv.Itoa(i)

		for j := 0; j < 20; j++ {
			name := "Datastore/statement/MySQL/City" + strconv.Itoa(j) + "/insert"
			data := [6]float64{31, 0.8240199999999998, 0.8240199999999998, 8e-05, 0.20662, 0.13516}
			mt.AddRaw(nil, name, "", data, Forced)
			mt.AddRaw(nil, name, scope, data, Forced)

			name = "WebTransaction/Uri/myblog2/newPost_rum_" + strconv.Itoa(j) + ".php"
			data = [6]float64{1, 0.00058, 0.00058, 0.00058, 0.00058, 0}
			mt.AddRaw(nil, name, "", data, Forced)
			mt.AddRaw(nil, name, scope, data, Forced)
		}
	}

	if data, err := mt.CollectorJSON(AgentRunID("12345"), time.Now()); err == nil {
		var v interface{}
		if err := json.Unmarshal(data, &v); err != nil {
			b.Fatalf("Unmarshal(CollectorJSON(...)) = %v", err)
		}
	} else {
		b.Fatalf("CollectorJSON(...) = %v", err)
	}

	b.ResetTimer()
	b.ReportAllocs()

	now := time.Now()
	for i := 0; i < b.N; i++ {
		mt.CollectorJSON(AgentRunID("12345"), now)
	}
}

func TestMetricTableHas(t *testing.T) {
	mt := NewMetricTable(20, start)
	mt.AddCount("foo", "", 1, 0)
	mt.AddCount("bar", "quux", 1, 0)

	if mt.Has("quux") {
		t.Fatal("non-existent metric is reported as existing")
	}

	if !mt.Has("foo") {
		t.Fatal("unscoped metric is reported as missing")
	}

	if !mt.Has("bar") {
		t.Fatal("scoped metric is reported as missing")
	}
}
