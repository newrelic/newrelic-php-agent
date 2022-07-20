//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package protocol

import flatbuffers "github.com/google/flatbuffers/go"

// EncodeEvent prepends a new Event object to the FlatBuffer and returns its offset.
func EncodeEvent(b *flatbuffers.Builder, data []byte) flatbuffers.UOffsetT {
	var dataOffset flatbuffers.UOffsetT

	if len(data) > 0 {
		dataOffset = b.CreateByteVector(data)
	}

	EventStart(b)
	EventAddData(b, dataOffset)
	return EventEnd(b)
}

// EncodeError prepends a new Error object to the FlatBuffer and returns
// its offset.
func EncodeError(b *flatbuffers.Builder, priority int32, data []byte) flatbuffers.UOffsetT {
	var dataOffset flatbuffers.UOffsetT

	if len(data) > 0 {
		dataOffset = b.CreateByteVector(data)
	}

	ErrorStart(b)
	ErrorAddPriority(b, priority)
	ErrorAddData(b, dataOffset)
	return ErrorEnd(b)
}

// EncodeMetric prepends a new Metric object to the FlatBuffer and returns
// its offset.
func EncodeMetric(b *flatbuffers.Builder, name string, data [6]float64,
	scoped, forced bool) flatbuffers.UOffsetT {
	nameOffset := b.CreateString(name)

	MetricStart(b)
	MetricAddName(b, nameOffset)

	var scopedBool bool
	var forcedBool bool

	if scoped {
		scopedBool = true
	}
	if forced {
		forcedBool = true
	}

	dataOffset := CreateMetricData(b, data[0], data[1], data[2], data[3],
		data[4], data[5], scopedBool, forcedBool)

	MetricAddData(b, dataOffset)

	return MetricEnd(b)
}

// EncodeSlowSQL prepends a new SlowSQL object to the FlatBuffer and returns
// its offset.
func EncodeSlowSQL(b *flatbuffers.Builder, id uint32, count int32,
	totalUS, minUS, maxUS uint64, metric, query string,
	params []byte) flatbuffers.UOffsetT {
	var metricOffset, queryOffset, paramsOffset flatbuffers.UOffsetT

	if len(params) > 0 {
		paramsOffset = b.CreateByteVector(params)
	}
	if len(query) > 0 {
		queryOffset = b.CreateString(query)
	}
	if len(metric) > 0 {
		metricOffset = b.CreateString(metric)
	}

	SlowSQLStart(b)
	SlowSQLAddId(b, id)
	SlowSQLAddCount(b, count)
	SlowSQLAddTotalMicros(b, totalUS)
	SlowSQLAddMinMicros(b, minUS)
	SlowSQLAddMaxMicros(b, maxUS)
	SlowSQLAddMetric(b, metricOffset)
	SlowSQLAddQuery(b, queryOffset)
	SlowSQLAddParams(b, paramsOffset)
	return SlowSQLEnd(b)
}

// EncodeTrace prepends a new Trace object to the FlatBuffer and returns
// its offset.
func EncodeTrace(b *flatbuffers.Builder, timestampMS, durationMS float64, guid string, force bool, data []byte) flatbuffers.UOffsetT {
	var dataOffset, guidOffset flatbuffers.UOffsetT

	if len(data) > 0 {
		dataOffset = b.CreateByteVector(data)
	}
	if len(guid) > 0 {
		guidOffset = b.CreateString(guid)
	}

	TraceStart(b)
	TraceAddTimestamp(b, timestampMS)
	TraceAddDuration(b, durationMS)
	TraceAddGuid(b, guidOffset)
	TraceAddData(b, dataOffset)
	if force {
		TraceAddForcePersist(b, true)
	}
	return TraceEnd(b)
}
