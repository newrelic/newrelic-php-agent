//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"errors"
	"fmt"
	"strconv"

	flatbuffers "github.com/google/flatbuffers/go"

	"newrelic/collector"
	"newrelic/limits"
	"newrelic/log"
	"newrelic/protocol"
)

type CommandsHandler struct {
	Processor AgentDataHandler
}

func aggregateMetrics(txn protocol.Transaction, h *Harvest, txnName string) {
	var m protocol.Metric
	var data protocol.MetricData
	var d [6]float64

	n := txn.MetricsLength()
	for i := 0; i < n; i++ {
		txn.Metrics(&m, i)
		m.Data(&data)

		d[0] = data.Count()
		d[1] = data.Total()
		d[2] = data.Exclusive()
		d[3] = data.Min()
		d[4] = data.Max()
		d[5] = data.SumSquares()

		forced := Unforced
		if data.Forced() != 0 {
			forced = Forced
		}

		metricName := m.Name()
		h.Metrics.AddRaw(metricName, "", "", d, forced)
		if data.Scoped() != 0 {
			h.Metrics.AddRaw(metricName, "", txnName, d, forced)
		}
	}
}

func copySlice(b []byte) []byte {
	if nil == b {
		return nil
	}
	cpy := make([]byte, len(b))
	copy(cpy, b)
	return cpy
}

type FlatTxn []byte

func (t FlatTxn) AggregateInto(h *Harvest) {
	var tbl flatbuffers.Table
	var txn protocol.Transaction
	var syntheticsResourceID string

	msg := protocol.GetRootAsMessage([]byte(t), 0)
	msg.Data(&tbl)
	txn.Init(tbl.Bytes, tbl.Pos)

	h.Metrics.AddValue("Supportability/TxnData/Size", "", float64(len(t)), Forced)
	h.Metrics.AddValue("Supportability/TxnData/CustomEvents", "", float64(txn.CustomEventsLength()), Forced)
	h.Metrics.AddValue("Supportability/TxnData/Metrics", "", float64(txn.MetricsLength()), Forced)
	h.Metrics.AddValue("Supportability/TxnData/SlowSQL", "", float64(txn.SlowSqlsLength()), Forced)

	txnName := string(txn.Name())
	requestURI := string(txn.Uri())
	samplingPriority := SamplingPriority(txn.SamplingPriority())

	if x := txn.SyntheticsResourceId(); len(x) > 0 {
		syntheticsResourceID = string(x)
	}

	pid := int(txn.Pid())
	if _, ok := h.pidSet[pid]; !ok {
		h.pidSet[pid] = struct{}{}
	}

	if event := txn.TxnEvent(nil); event != nil {
		cpy := copySlice(event.Data())
		if syntheticsResourceID == "" {
			h.TxnEvents.AddTxnEvent(cpy, samplingPriority)
		} else {
			h.TxnEvents.AddSyntheticsEvent(cpy, samplingPriority)
		}
	}

	aggregateMetrics(txn, h, txnName)

	if n := txn.ErrorsLength(); n > 0 {
		var e protocol.Error

		for i := 0; i < n; i++ {
			txn.Errors(&e, i)

			priority := int(e.Priority())
			dataNeedsCopy := e.Data()
			h.Errors.AddError(priority, dataNeedsCopy)
		}
	}

	if n := txn.SlowSqlsLength(); n > 0 {
		var slowSQL protocol.SlowSQL

		for i := 0; i < n; i++ {
			txn.SlowSqls(&slowSQL, i)

			slow := &SlowSQL{}
			slow.ID = SQLId(slowSQL.Id())
			slow.Count = slowSQL.Count()
			slow.TotalMicros = slowSQL.TotalMicros()
			slow.MinMicros = slowSQL.MinMicros()
			slow.MaxMicros = slowSQL.MaxMicros()

			slow.MetricName = string(slowSQL.Metric())
			slow.Query = string(slowSQL.Query())
			slow.TxnName = txnName
			slow.TxnURL = requestURI
			slow.Params = copySlice(slowSQL.Params())

			h.SlowSQLs.Observe(slow)
		}
	}

	if n := txn.CustomEventsLength(); n > 0 {
		var e protocol.Event

		for i := 0; i < n; i++ {
			txn.CustomEvents(&e, i)
			data := copySlice(e.Data())
			h.CustomEvents.AddEventFromData(data, samplingPriority)
		}
	}

	if n := txn.SpanEventsLength(); n > 0 {
		var e protocol.Event

		for i := 0; i < n; i++ {
			txn.SpanEvents(&e, i)
			data := copySlice(e.Data())
			h.SpanEvents.AddEventFromData(data, samplingPriority)
		}
	}

	if trace := txn.Trace(nil); trace != nil {
		data := trace.Data()
		tt := &TxnTrace{
			UnixTimestampMillis:  trace.Timestamp(),
			DurationMillis:       trace.Duration(),
			GUID:                 string(trace.Guid()),
			SyntheticsResourceID: syntheticsResourceID,
			ForcePersist:         trace.ForcePersist() != 0,
			MetricName:           txnName,
			RequestURI:           requestURI,
		}

		h.Metrics.AddValue("Supportability/TxnData/TraceSize", "",
			float64(len(data)), Forced)

		if h.TxnTraces.IsKeeper(tt) {
			tt.Data = copySlice(data)
			h.TxnTraces.AddTxnTrace(tt)
		}
	}

	if n := txn.ErrorEventsLength(); n > 0 {
		var e protocol.Event

		for i := 0; i < n; i++ {
			txn.ErrorEvents(&e, i)
			data := copySlice(e.Data())
			h.ErrorEvents.AddEventFromData(data, samplingPriority)
		}
	}
}

func MarshalAppInfoReply(reply AppInfoReply) []byte {
	buf := flatbuffers.NewBuilder(0)
	if reply.RunIDValid {
		protocol.AppReplyStart(buf)
		protocol.AppReplyAddStatus(buf, protocol.AppStatusStillValid)
		dataOffset := protocol.AppReplyEnd(buf)

		protocol.MessageStart(buf)
		protocol.MessageAddDataType(buf, protocol.MessageBodyAppReply)
		protocol.AppReplyAddConnectTimestamp(buf, reply.ConnectTimestamp)
		protocol.AppReplyAddHarvestFrequency(buf, reply.HarvestFrequency)
		protocol.AppReplyAddSamplingTarget(buf, reply.SamplingTarget)
		protocol.MessageAddData(buf, dataOffset)
		buf.Finish(protocol.MessageEnd(buf))
	} else if reply.State == AppStateConnected {
		replyPos := buf.CreateByteVector(reply.ConnectReply)
		replyPoliciesPos := buf.CreateByteVector(reply.SecurityPolicies)
		protocol.AppReplyStart(buf)
		protocol.AppReplyAddStatus(buf, protocol.AppStatusConnected)
		protocol.AppReplyAddConnectReply(buf, replyPos)
		protocol.AppReplyAddSecurityPolicies(buf, replyPoliciesPos)
		protocol.AppReplyAddConnectTimestamp(buf, reply.ConnectTimestamp)
		protocol.AppReplyAddHarvestFrequency(buf, reply.HarvestFrequency)
		protocol.AppReplyAddSamplingTarget(buf, reply.SamplingTarget)
		dataOffset := protocol.AppReplyEnd(buf)

		protocol.MessageStart(buf)
		protocol.MessageAddDataType(buf, protocol.MessageBodyAppReply)
		protocol.MessageAddData(buf, dataOffset)
		buf.Finish(protocol.MessageEnd(buf))
	} else {
		protocol.AppReplyStart(buf)
		switch reply.State {
		case AppStateUnknown:
			protocol.AppReplyAddStatus(buf, protocol.AppStatusUnknown)
		//case AppStateInvalidLicense:
		//	protocol.AppReplyAddStatus(buf, protocol.AppStatusInvalidLicense)
		case AppStateDisconnected:
			protocol.AppReplyAddStatus(buf, protocol.AppStatusDisconnected)
		}
		dataOffset := protocol.AppReplyEnd(buf)

		protocol.MessageStart(buf)
		protocol.MessageAddDataType(buf, protocol.MessageBodyAppReply)
		protocol.MessageAddData(buf, dataOffset)
		buf.Finish(protocol.MessageEnd(buf))
	}

	return buf.Bytes[buf.Head():]
}

func UnmarshalAppInfo(tbl flatbuffers.Table) *AppInfo {
	var app protocol.App

	app.Init(tbl.Bytes, tbl.Pos)

	policies := AgentPolicies{}
	json.Unmarshal(copySlice(app.SupportedSecurityPolicies()), &policies.Policies)

	info := &AppInfo{
		License:                   collector.LicenseKey(app.License()),
		Appname:                   string(app.AppName()),
		AgentLanguage:             string(app.AgentLanguage()),
		AgentVersion:              string(app.AgentVersion()),
		RedirectCollector:         string(app.RedirectCollector()),
		Environment:               JSONString(copySlice(app.Environment())),
		Labels:                    JSONString(copySlice(app.Labels())),
		Hostname:                  string(app.Host()),
		HostDisplayName:           string(app.DisplayHost()),
		SecurityPolicyToken:       string(app.SecurityPolicyToken()),
		SupportedSecurityPolicies: policies,
		TraceObserverHost:         string(app.TraceObserverHost()),
		TraceObserverPort:         app.TraceObserverPort(),
		SpanQueueSize:             app.SpanQueueSize(),
		HighSecurity:              app.HighSecurity(),

	}

	info.initSettings(app.Settings())

	// Of the four Event Limits (span, custom, analytic and error),
	// only span events is configurable from the agent.
	// If this changes in the future, the other values can be added here.
	info.AgentEventLimits.SpanEventConfig.Limit = int(app.SpanEventsMaxSamplesStored())

	return info
}

func processBinary(data []byte, handler AgentDataHandler) ([]byte, error) {
	if len(data) == 0 {
		log.Debugf("ignoring empty message")
		return nil, nil
	}

	log.Debugf("received binary message, len=%d", len(data))

	// Check that the first offset is actually within the bounds of the message
	// length.
	offset := int(flatbuffers.GetUOffsetT(data[0:]))
	if len(data)-limits.MinFlatbufferSize <= offset {
		return nil, errors.New("offset is too large, len=" + strconv.Itoa(offset))
	}

	msg := protocol.GetRootAsMessage(data, 0)

	switch msg.DataType() {
	case protocol.MessageBodyTransaction:
		var tbl flatbuffers.Table

		if !msg.Data(&tbl) {
			return nil, errors.New("transaction missing message body")
		}

		if id := msg.AgentRunId(); len(id) > 0 {
			// Send the data directly to the processor without a
			// copy because each message is in its own buffer.
			handler.IncomingTxnData(AgentRunID(id), FlatTxn(data))
			return nil, nil
		}
		return nil, errors.New("missing agent run id for txn data command")

	case protocol.MessageBodyApp:
		var tbl flatbuffers.Table

		if !msg.Data(&tbl) {
			return nil, errors.New("app missing message body")
		}

		info := UnmarshalAppInfo(tbl)

		var runID *AgentRunID
		if id := msg.AgentRunId(); id != nil {
			r := AgentRunID(id)
			runID = &r
		}

		reply := handler.IncomingAppInfo(runID, info)

		return MarshalAppInfoReply(reply), nil

	case protocol.MessageBodySpanBatch:
		var tbl flatbuffers.Table

		if !msg.Data(&tbl) {
			return nil, errors.New("span batch missing message body")
		}

		var batch protocol.SpanBatch
		batch.Init(tbl.Bytes, tbl.Pos)

		id := msg.AgentRunId()
		if len(id) == 0 {
			return nil, errors.New("missing agent run id for span batch command")
		}

		spanBatch := SpanBatch{id: AgentRunID(id), count: batch.Count(), batch: batch.EncodedBytes()}

		handler.IncomingSpanBatch(spanBatch)

		return nil, nil

	case protocol.MessageBodyNONE:
		log.Debugf("ignoring None message")
		return nil, nil

	case protocol.MessageBodyAppReply:
		log.Debugf("message is AppReply")
		return nil, nil

	default:
		return nil, errors.New("binary encoding not implemented")
	}
}

func (h CommandsHandler) HandleMessage(msg RawMessage) ([]byte, error) {
	switch mt := msg.Type; mt {
	case MessageTypeBinary:
		return processBinary(msg.Bytes, h.Processor)

	default:
		return nil, fmt.Errorf("unsupported message encoding: %v", mt)
	}
}
