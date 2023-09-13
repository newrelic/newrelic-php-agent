//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"fmt"
	"testing"

	"newrelic.com/daemon/newrelic/limits"
)

func TestBasic(t *testing.T) {
	events := newAnalyticsEvents(10)
	events.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})
	events.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})
	events.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), priority: SamplingPriority(0.8)})

	id := AgentRunID(`12345`)
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}

	expected := `["12345",` +
		`{"reservoir_size":10,"events_seen":3},` +
		`[[{"x":1},{},{}],` +
		`[{"x":1},{},{}],` +
		`[{"x":1},{},{}]]]`

	if string(json) != expected {
		t.Error(string(json))
	}
	if 3 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 3 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func TestEmpty(t *testing.T) {
	events := newAnalyticsEvents(10)
	id := AgentRunID(`12345`)
	empty := events.Empty()
	if !empty {
		t.Fatal(empty)
	}
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":10,"events_seen":0},[]]` {
		t.Error(string(json))
	}
	if 0 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 0 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func sampleAnalyticsEvent(priority SamplingPriority) AnalyticsEvent {
	return AnalyticsEvent{
		priority: priority,
		data:     []byte(fmt.Sprintf(`{"x":%f}`, priority)),
	}
}

func TestSampling(t *testing.T) {
	events := newAnalyticsEvents(3)
	events.AddEvent(sampleAnalyticsEvent(0.999999))
	events.AddEvent(sampleAnalyticsEvent(0.1))
	events.AddEvent(sampleAnalyticsEvent(0.9))
	events.AddEvent(sampleAnalyticsEvent(0.2))
	events.AddEvent(sampleAnalyticsEvent(0.8))
	events.AddEvent(sampleAnalyticsEvent(0.3))

	id := AgentRunID(`12345`)
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":3,"events_seen":6},[{"x":0.800000},{"x":0.999999},{"x":0.900000}]]` {
		t.Error(string(json))
	}
	if 6 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 3 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func TestMergeEmpty(t *testing.T) {
	e1 := newAnalyticsEvents(10)
	e2 := newAnalyticsEvents(10)
	e1.Merge(e2)
	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":10,"events_seen":0},[]]` {
		t.Error(string(json))
	}
	if 0 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 0 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
}

func TestMergeFull(t *testing.T) {
	e1 := newAnalyticsEvents(2)
	e2 := newAnalyticsEvents(3)

	e1.AddEvent(sampleAnalyticsEvent(0.5))
	e1.AddEvent(sampleAnalyticsEvent(0.1))
	e1.AddEvent(sampleAnalyticsEvent(0.15))

	e2.AddEvent(sampleAnalyticsEvent(0.18))
	e2.AddEvent(sampleAnalyticsEvent(0.12))
	e2.AddEvent(sampleAnalyticsEvent(0.6))
	e2.AddEvent(sampleAnalyticsEvent(0.24))

	e1.Merge(e2)
	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":2,"events_seen":7},[{"x":0.500000},{"x":0.600000}]]` {
		t.Error(string(json))
	}
	if 7 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 2 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
}

func TestAnalyticsEventMergeFailedSuccess(t *testing.T) {
	e1 := newAnalyticsEvents(2)
	e2 := newAnalyticsEvents(3)

	e1.AddEvent(sampleAnalyticsEvent(0.5))
	e1.AddEvent(sampleAnalyticsEvent(0.10))
	e1.AddEvent(sampleAnalyticsEvent(0.15))

	e2.AddEvent(sampleAnalyticsEvent(0.18))
	e2.AddEvent(sampleAnalyticsEvent(0.12))
	e2.AddEvent(sampleAnalyticsEvent(0.6))
	e2.AddEvent(sampleAnalyticsEvent(0.24))

	e1.MergeFailed(e2)

	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":2,"events_seen":7},[{"x":0.500000},{"x":0.600000}]]` {
		t.Error(string(json))
	}
	if 7 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 2 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
	if 1 != e1.failedHarvests {
		t.Error(e1.failedHarvests)
	}
}

func TestAnalyticsEventMergeFailedLimitReached(t *testing.T) {
	e1 := newAnalyticsEvents(2)
	e2 := newAnalyticsEvents(3)

	e1.AddEvent(sampleAnalyticsEvent(0.5))
	e1.AddEvent(sampleAnalyticsEvent(0.10))
	e1.AddEvent(sampleAnalyticsEvent(0.15))

	e2.AddEvent(sampleAnalyticsEvent(0.18))
	e2.AddEvent(sampleAnalyticsEvent(0.12))
	e2.AddEvent(sampleAnalyticsEvent(0.6))
	e2.AddEvent(sampleAnalyticsEvent(0.24))

	e2.failedHarvests = limits.FailedEventsAttemptsLimit

	e1.MergeFailed(e2)

	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":2,"events_seen":3},[{"x":0.150000},{"x":0.500000}]]` {
		t.Error(string(json))
	}
	if 3 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 2 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
	if 0 != e1.failedHarvests {
		t.Error(e1.failedHarvests)
	}
}

func BenchmarkEventsCollectorJSON(b *testing.B) {
	// Let's not rely on a computationally intensive random number generator
	// for this benchmark.  AddTxnEvent is not responsible for creating a
	// random sampling priority.  Let's just benchmark its allocation of
	// transaction events and supply it a rotating set of sampling
	// priorities.
	sp := []SamplingPriority{0.99999, 0.42, 0.13, 0.007, 0.8}
	data := []byte(`[{"zip":"zap","alpha":"beta","pen":"pencil"},{},{}]`)
	events := NewTxnEvents(limits.MaxTxnEvents)

	for n := 0; n < limits.MaxTxnEvents; n++ {
		events.AddTxnEvent(data, sp[n%len(sp)])
	}

	id := AgentRunID("12345")

	b.ReportAllocs()
	b.ResetTimer()

	for n := 0; n < b.N; n++ {
		js, err := events.CollectorJSON(id)
		if nil != err {
			b.Fatal(err, js)
		}
	}
}
