//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"container/heap"
	"encoding/json"
	"time"
)

// Error is the datatype representing an error or exception captured by the
// instrumented application.  Errors are instance data and are not aggregated
// together in any way.  Therefore, the final JSON expected by the collector is
// created by the agent and is sent to the daemon complete.
type Error struct {
	// Priority indicates which errors should be saved in the event that the
	// the number of errors is larger than the limit.  Higher numbers have
	// priority.  This field is not sent to the collector.
	Priority int
	// Data contains the error JSON as it is expected by the collector.
	Data JSONString
}

// ErrorHeap is a bounded collection of Errors captured by an
// instrumented application. Once the collection is full, replacement
// occurs based on the relative priorities of the errors.
type ErrorHeap []*Error

func (h ErrorHeap) Len() int           { return len(h) }
func (h ErrorHeap) Less(i, j int) bool { return h[i].Priority < h[j].Priority }
func (h ErrorHeap) Swap(i, j int)      { h[i], h[j] = h[j], h[i] }

// Push appends x to the collection. This method should not be called
// directly because it does not enforce the maximum capacity. Use AddError
// instead.
func (h *ErrorHeap) Push(x interface{}) {
	*h = append(*h, x.(*Error))
}

// Pop removes the lowest priority element from the error heap.
func (h *ErrorHeap) Pop() interface{} {
	old := *h
	n := len(old)
	x := old[n-1]
	*h = old[0 : n-1]
	return x
}

// NewErrorHeap returns a new ErrorHeap with maximum capacity max.
func NewErrorHeap(max int) *ErrorHeap {
	h := make(ErrorHeap, 0, max)
	heap.Init(&h)
	return &h
}

// AddError observes an error captured by an application. If the
// collection is full, replacement is performed.
func (h *ErrorHeap) AddError(priority int, dataNeedsCopy []byte) {
	if len(*h) == cap(*h) {
		// When a tie occurs sampling decisions are made by prioritizing the
		// events added to the heap first.
		if priority <= (*h)[0].Priority {
			return
		}
		heap.Pop(h)
	}

	data := make([]byte, len(dataNeedsCopy))
	copy(data, dataNeedsCopy)
	heap.Push(h, &Error{Priority: priority, Data: data})
}

// MarshalJSON marshals e to JSON according to the schema expected
// by the collector.
func (e *Error) MarshalJSON() ([]byte, error) {
	return json.Marshal(e.Data)
}

// Empty returns true if the collection is empty.
func (h *ErrorHeap) Empty() bool {
	return 0 == h.Len()
}

// Data marshals the collection to JSON according to the schema expected
// by the collector.
func (h *ErrorHeap) Data(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return json.Marshal([]interface{}{id, *h})
}

// Audit marshals the collection to JSON according to the schema
// expected by the audit log. For traced errors, the audit schema is the
// same as the schema expected by the collector.
func (h *ErrorHeap) Audit(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return nil, nil
}

// FailedHarvest is a callback invoked by the processor when an attempt to
// deliver the collection to the collector fails. Traced errors are
// discarded after one failed attempt.
func (h *ErrorHeap) FailedHarvest(newHarvest *Harvest) {}
