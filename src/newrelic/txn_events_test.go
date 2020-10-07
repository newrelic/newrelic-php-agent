//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"

	"newrelic/limits"
)

func TestSyntheticsSampling(t *testing.T) {
	events := NewTxnEvents(1)

	events.AddEvent(AnalyticsEvent{
		priority: 1.99999,
		data:     []byte(`{"transaction_event"}`),
	})

	// The priority sampling algorithm is implemented using isLowerPriority().  In
	// the case of an event pool with a single event, an incoming event with the
	// same priority would kick out the event already in the pool.  To really test
	// whether synthetics are given highest deference, add a synthetics event
	// with a really low priority and affirm it kicks out the event already in the
	// pool.
	events.AddSyntheticsEvent([]byte(`{"analytic_event"}`), 0.0)

	id := AgentRunID(`12345`)
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":1,"events_seen":2},[{"analytic_event"}]]` {
		t.Error(string(json))
	}
	if 2 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 1 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func BenchmarkTxnEvents(b *testing.B) {
	// Let's not rely on a computationally intensive random number generator
	// for this benchmark.  AddTxnEvent is not responsible for creating a
	// random sampling priority.  Let's just benchmark its allocation of
	// transaction events and supply it a rotating set of sampling
	// priorities.
	sp := []SamplingPriority{0.99999, 0.42, 0.13, 0.007, 0.8}
	data := []byte(`[{"zip":"zap","alpha":"beta","pen":"pencil"},{},{}]`)
	events := NewTxnEvents(limits.MaxTxnEvents)

	b.ReportAllocs()

	for n := 0; n < b.N; n++ {
		events.AddTxnEvent(data, sp[n%len(sp)])
	}
}
