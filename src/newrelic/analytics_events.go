//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"container/heap"
	"encoding/json"
	"time"

	"newrelic/limits"
	"newrelic/log"
)

// AnalyticsEvent represents an analytics event reported by an agent.
type AnalyticsEvent struct {
	priority SamplingPriority
	data     JSONString
}

// analyticsEventHeap implements a min-heap of analytics events according
// to their event stamps.
type analyticsEventHeap []AnalyticsEvent

// analyticsEvents represents a bounded collection of analytics events
// reported by agents. When the collection is full, priority sampling
// is used as the replacement strategy.
type analyticsEvents struct {
	numSeen        int
	events         *analyticsEventHeap
	failedHarvests int
}

// Split splits the events into two.  NOTE! The two event pools are not valid
// priority queues, and should only be used to create JSON, not for adding any
// events.
func (events *analyticsEvents) Split() (*analyticsEvents, *analyticsEvents) {
	eventHeap := *events.events
	eventHeap1 := make(analyticsEventHeap, len(eventHeap)/2)
	eventHeap2 := make(analyticsEventHeap, len(eventHeap)-len(eventHeap1))

	e1 := &analyticsEvents{
		numSeen:        len(eventHeap1),
		events:         &eventHeap1,
		failedHarvests: events.failedHarvests,
	}
	e2 := &analyticsEvents{
		numSeen:        len(eventHeap2),
		events:         &eventHeap2,
		failedHarvests: events.failedHarvests,
	}

	// Note that slicing is not used to ensure that length == capacity for
	// e1.events and e2.events.
	copy(*e1.events, eventHeap)
	copy(*e2.events, eventHeap[len(eventHeap)/2:])

	return e1, e2
}

// NumSeen returns the total number of analytics events observed.
func (events *analyticsEvents) NumSeen() float64 {
	return float64(events.numSeen)
}

// NumSaved returns the number of analytics events in the reservoir.
func (events *analyticsEvents) NumSaved() float64 {
	return float64(len(*events.events))
}

func (h analyticsEventHeap) Len() int           { return len(h) }
func (h analyticsEventHeap) Less(i, j int) bool { return h[i].priority.IsLowerPriority(h[j].priority) }
func (h analyticsEventHeap) Swap(i, j int)      { h[i], h[j] = h[j], h[i] }

// Push appends x to the heap. This method should not be called
// directly because it does not maintain the min-heap property, and
// it does not enforce the maximum capacity. Use AddEvent instead.
func (h *analyticsEventHeap) Push(x interface{}) {
	*h = append(*h, x.(AnalyticsEvent))
}

// Pop removes and returns the analytics event with the lowest priority.
func (h *analyticsEventHeap) Pop() interface{} {
	old := *h
	n := len(old)
	x := old[n-1]
	*h = old[0 : n-1]
	return x
}

// newAnalyticsEvents returns a new event reservoir with capacity max.
func newAnalyticsEvents(max int) *analyticsEvents {
	h := make(analyticsEventHeap, 0, max)
	return &analyticsEvents{
		numSeen:        0,
		events:         &h,
		failedHarvests: 0,
	}
}

// AddEvent observes the occurrence of an analytics event. If the
// reservoir is full, sampling occurs. Note, when sampling occurs, it
// is possible the event may be discarded instead of added.
func (events *analyticsEvents) AddEvent(e AnalyticsEvent) {
	events.numSeen++

	if len(*events.events) < cap(*events.events) {
		events.events.Push(e)
		if len(*events.events) == cap(*events.events) {
			// Delay heap initialization so that we can have deterministic
			// ordering for integration tests (the max is not being reached).
			heap.Init(events.events)
		}
		return
	}

	// zero is a valid capacity because it signifies events are disabled
	if 0 == cap(*events.events) {
		return
	}

	if e.priority.IsLowerPriority((*events.events)[0].priority) {
		return
	}

	heap.Pop(events.events)
	heap.Push(events.events, e)
}

// MergeFailed merges the analytics events contained in other into
// events after a failed delivery attempt. If FailedEventsAttemptsLimit
// attempts have been made, the events in other are discarded. If events
// is full, reservoir sampling is performed.
func (events *analyticsEvents) MergeFailed(other *analyticsEvents) {
	fails := other.failedHarvests + 1
	if fails > limits.FailedEventsAttemptsLimit {
		log.Debugf("discarding events: %d failed harvest attempts", fails)
		return
	}
	log.Debugf("merging events: %d failed harvest attempts", fails)
	events.failedHarvests = fails
	events.Merge(other)
}

// Merge merges the analytics events contained in other into events.
// If the combined number of events exceeds the maximum capacity of
// events, reservoir sampling with uniform distribution is performed.
func (events *analyticsEvents) Merge(other *analyticsEvents) {
	allSeen := events.numSeen + other.numSeen

	for _, e := range *other.events {
		events.AddEvent(e)
	}
	events.numSeen = allSeen
}

// CollectorJSON marshals events to JSON according to the schema expected
// by the collector.
func (events *analyticsEvents) CollectorJSON(id AgentRunID) ([]byte, error) {
	buf := &bytes.Buffer{}

	es := *events.events

	samplingData := struct {
		ReservoirSize int `json:"reservoir_size"`
		EventsSeen    int `json:"events_seen"`
	}{
		ReservoirSize: cap(es),
		EventsSeen:    events.numSeen,
	}

	estimate := len(es) * 128
	buf.Grow(estimate)
	buf.WriteByte('[')

	enc := json.NewEncoder(buf)
	if err := enc.Encode(id); err != nil {
		return nil, err
	}
	// replace trailing newline
	buf.Bytes()[buf.Len()-1] = ','

	if err := enc.Encode(samplingData); err != nil {
		return nil, err
	}

	buf.Bytes()[buf.Len()-1] = ','

	buf.WriteByte('[')
	for i := 0; i < len(es); i++ {
		if i > 0 {
			buf.WriteByte(',')
		}
		buf.Write(es[i].data)
	}
	buf.WriteByte(']')
	buf.WriteByte(']')

	return buf.Bytes(), nil
}

// Empty returns true if the collection is empty.
func (events *analyticsEvents) Empty() bool {
	return 0 == events.events.Len()
}

// Data marshals the collection to JSON according to the schema expected
// by the collector.
func (events *analyticsEvents) Data(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return events.CollectorJSON(id)
}

// Audit marshals the collection to JSON according to the schema
// expected by the audit log. For analytics events, the audit schema is
// the same as the schema expected by the collector.
func (events *analyticsEvents) Audit(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return nil, nil
}
