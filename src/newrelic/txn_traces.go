//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"container/heap"
	"encoding/json"
	"time"

	"newrelic/collector"
	"newrelic/limits"
)

type TxnTrace struct {
	MetricName           string
	RequestURI           string
	UnixTimestampMillis  float64
	DurationMillis       float64
	Data                 JSONString
	GUID                 string
	ForcePersist         bool
	SyntheticsResourceID string
}

func (t *TxnTrace) collectorData(compressEncode bool) interface{} {
	if !compressEncode {
		return t.Data
	}

	p, err := collector.CompressEncode(t.Data)
	if nil != err {
		return nil
	}
	return p
}

func (t *TxnTrace) collectorJSON(compressEncode bool) []interface{} {
	var resourceID interface{}

	if "" != t.SyntheticsResourceID {
		resourceID = t.SyntheticsResourceID
	}

	return []interface{}{
		t.UnixTimestampMillis, // milliseconds
		t.DurationMillis,      // milliseconds
		t.MetricName,
		t.RequestURI,
		t.collectorData(compressEncode),
		t.GUID,
		nil, // reserved for future use
		t.ForcePersist,
		nil, // X-Ray sessions not supported
		resourceID,
	}
}

type TxnTraceHeap []*TxnTrace

func (h *TxnTraceHeap) isEmpty() bool {
	return 0 == len(*h)
}

func NewTxnTraceHeap(max int) *TxnTraceHeap {
	h := make(TxnTraceHeap, 0, max)
	heap.Init(&h)
	return &h
}

func (h TxnTraceHeap) Len() int            { return len(h) }
func (h TxnTraceHeap) Less(i, j int) bool  { return h[i].DurationMillis < h[j].DurationMillis }
func (h TxnTraceHeap) Swap(i, j int)       { h[i], h[j] = h[j], h[i] }
func (h *TxnTraceHeap) Push(x interface{}) { *h = append(*h, x.(*TxnTrace)) }

func (h *TxnTraceHeap) Pop() interface{} {
	old := *h
	n := len(old)
	x := old[n-1]
	*h = old[0 : n-1]
	return x
}

func (h *TxnTraceHeap) IsKeeper(tt *TxnTrace) bool {
	if len(*h) < cap(*h) {
		return true
	}
	return tt.DurationMillis >= (*h)[0].DurationMillis
}

func (h *TxnTraceHeap) AddTxnTrace(t *TxnTrace) {
	if len(*h) < cap(*h) {
		heap.Push(h, t)
		return
	}

	// Keep the oldest when durations are equal.
	if t.DurationMillis < (*h)[0].DurationMillis {
		return
	}
	heap.Pop(h)
	heap.Push(h, t)
}

type TxnTraces struct {
	regular        *TxnTraceHeap
	forcePersisted *TxnTraceHeap
	synthetics     *TxnTraceHeap
}

func NewTxnTraces() *TxnTraces {
	return &TxnTraces{
		regular:        NewTxnTraceHeap(limits.MaxRegularTraces),
		forcePersisted: NewTxnTraceHeap(limits.MaxForcePersistTraces),
		synthetics:     NewTxnTraceHeap(limits.MaxSyntheticsTraces),
	}
}

func (traces *TxnTraces) IsKeeper(tt *TxnTrace) bool {
	switch {
	case tt.SyntheticsResourceID != "":
		return traces.synthetics.IsKeeper(tt)
	case tt.ForcePersist:
		return traces.forcePersisted.IsKeeper(tt)
	default:
		return traces.regular.IsKeeper(tt)
	}
}

func (traces *TxnTraces) AddTxnTrace(t *TxnTrace) {
	// The trick here is figuring out which trace set the trace in question
	// should go into. Synthetics traces "win" here: if the transaction is
	// related to a synthetics transaction at all, we want it in that pool.
	if "" != t.SyntheticsResourceID {
		traces.synthetics.AddTxnTrace(t)
	} else if t.ForcePersist {
		traces.forcePersisted.AddTxnTrace(t)
	} else {
		traces.regular.AddTxnTrace(t)
	}
}

func (h *TxnTraceHeap) collectorJSON(compressEncode bool) []interface{} {
	arr := make([]interface{}, len(*h))
	for i, t := range *h {
		arr[i] = t.collectorJSON(compressEncode)
	}
	return arr
}

func (traces *TxnTraces) Empty() bool {
	return traces.synthetics.isEmpty() &&
		traces.forcePersisted.isEmpty() &&
		traces.regular.isEmpty()
}

func (traces *TxnTraces) Data(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return traces.CollectorJSON(id, true)
}
func (traces *TxnTraces) Audit(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return traces.CollectorJSON(id, false)
}
func (traces *TxnTraces) FailedHarvest(newHarvest *Harvest) {}

func (traces *TxnTraces) CollectorJSON(id AgentRunID, compressEncode bool) ([]byte, error) {
	inner := traces.synthetics.collectorJSON(compressEncode)
	inner = append(inner, traces.forcePersisted.collectorJSON(compressEncode)...)
	inner = append(inner, traces.regular.collectorJSON(compressEncode)...)

	return json.Marshal([]interface{}{id, inner})
}
