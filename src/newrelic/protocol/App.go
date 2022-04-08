// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type App struct {
	_tab flatbuffers.Table
}

func GetRootAsApp(buf []byte, offset flatbuffers.UOffsetT) *App {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &App{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsApp(buf []byte, offset flatbuffers.UOffsetT) *App {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &App{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *App) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *App) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *App) License() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) AppName() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) AgentLanguage() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(8))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) AgentVersion() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(10))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) HighSecurity() bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(12))
	if o != 0 {
		return rcv._tab.GetBool(o + rcv._tab.Pos)
	}
	return false
}

func (rcv *App) MutateHighSecurity(n bool) bool {
	return rcv._tab.MutateBoolSlot(12, n)
}

func (rcv *App) RedirectCollector() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(14))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) Environment() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(16))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) Settings() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(18))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) Labels() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(20))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) DisplayHost() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(22))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) SecurityPolicyToken() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(24))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) SupportedSecurityPolicies() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(26))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) Host() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(28))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) TraceObserverHost() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(30))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *App) TraceObserverPort() uint16 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(32))
	if o != 0 {
		return rcv._tab.GetUint16(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *App) MutateTraceObserverPort(n uint16) bool {
	return rcv._tab.MutateUint16Slot(32, n)
}

func (rcv *App) SpanQueueSize() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(34))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *App) MutateSpanQueueSize(n uint64) bool {
	return rcv._tab.MutateUint64Slot(34, n)
}

func (rcv *App) SpanEventsMaxSamplesStored() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(36))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *App) MutateSpanEventsMaxSamplesStored(n uint64) bool {
	return rcv._tab.MutateUint64Slot(36, n)
}

func AppStart(builder *flatbuffers.Builder) {
	builder.StartObject(17)
}
func AppAddLicense(builder *flatbuffers.Builder, license flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(0, flatbuffers.UOffsetT(license), 0)
}
func AppAddAppName(builder *flatbuffers.Builder, appName flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(1, flatbuffers.UOffsetT(appName), 0)
}
func AppAddAgentLanguage(builder *flatbuffers.Builder, agentLanguage flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(2, flatbuffers.UOffsetT(agentLanguage), 0)
}
func AppAddAgentVersion(builder *flatbuffers.Builder, agentVersion flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(3, flatbuffers.UOffsetT(agentVersion), 0)
}
func AppAddHighSecurity(builder *flatbuffers.Builder, highSecurity bool) {
	builder.PrependBoolSlot(4, highSecurity, false)
}
func AppAddRedirectCollector(builder *flatbuffers.Builder, redirectCollector flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(5, flatbuffers.UOffsetT(redirectCollector), 0)
}
func AppAddEnvironment(builder *flatbuffers.Builder, environment flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(6, flatbuffers.UOffsetT(environment), 0)
}
func AppAddSettings(builder *flatbuffers.Builder, settings flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(7, flatbuffers.UOffsetT(settings), 0)
}
func AppAddLabels(builder *flatbuffers.Builder, labels flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(8, flatbuffers.UOffsetT(labels), 0)
}
func AppAddDisplayHost(builder *flatbuffers.Builder, displayHost flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(9, flatbuffers.UOffsetT(displayHost), 0)
}
func AppAddSecurityPolicyToken(builder *flatbuffers.Builder, securityPolicyToken flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(10, flatbuffers.UOffsetT(securityPolicyToken), 0)
}
func AppAddSupportedSecurityPolicies(builder *flatbuffers.Builder, supportedSecurityPolicies flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(11, flatbuffers.UOffsetT(supportedSecurityPolicies), 0)
}
func AppAddHost(builder *flatbuffers.Builder, host flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(12, flatbuffers.UOffsetT(host), 0)
}
func AppAddTraceObserverHost(builder *flatbuffers.Builder, traceObserverHost flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(13, flatbuffers.UOffsetT(traceObserverHost), 0)
}
func AppAddTraceObserverPort(builder *flatbuffers.Builder, traceObserverPort uint16) {
	builder.PrependUint16Slot(14, traceObserverPort, 0)
}
func AppAddSpanQueueSize(builder *flatbuffers.Builder, spanQueueSize uint64) {
	builder.PrependUint64Slot(15, spanQueueSize, 0)
}
func AppAddSpanEventsMaxSamplesStored(builder *flatbuffers.Builder, spanEventsMaxSamplesStored uint64) {
	builder.PrependUint64Slot(16, spanEventsMaxSamplesStored, 0)
}
func AppEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
