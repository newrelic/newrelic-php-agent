//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//
// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type Event struct {
	_tab flatbuffers.Table
}

func GetRootAsEvent(buf []byte, offset flatbuffers.UOffsetT) *Event {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &Event{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsEvent(buf []byte, offset flatbuffers.UOffsetT) *Event {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &Event{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *Event) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *Event) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *Event) Data() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func EventStart(builder *flatbuffers.Builder) {
	builder.StartObject(1)
}
func EventAddData(builder *flatbuffers.Builder, data flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(0, flatbuffers.UOffsetT(data), 0)
}
func EventEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
