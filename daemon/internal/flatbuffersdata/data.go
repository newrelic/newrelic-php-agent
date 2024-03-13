//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package flatbuffersdata

import (
	"encoding/json"
	"os"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/protocol"

	flatbuffers "github.com/google/flatbuffers/go"
)

func MarshalAppInfo(info *newrelic.AppInfo) ([]byte, error) {
	settingsJSON, _ := json.Marshal(info.Settings)
	envJSON, _ := json.Marshal(info.Environment)
	labelsJSON, _ := json.Marshal(info.Labels)
	metadataJSON, _ := json.Marshal(info.Metadata)

	buf := flatbuffers.NewBuilder(0)

	license := buf.CreateString(string(info.License))
	appname := buf.CreateString(info.Appname)
	lang := buf.CreateString(info.AgentLanguage)
	version := buf.CreateString(info.AgentVersion)
	collector := buf.CreateString(info.RedirectCollector)
	settings := buf.CreateString(string(settingsJSON))
	env := buf.CreateString(string(envJSON))
	labels := buf.CreateString(string(labelsJSON))
	metadata := buf.CreateString(string(metadataJSON))
	host := buf.CreateString(string(info.Hostname))
	traceObserverHost := buf.CreateString(info.TraceObserverHost)
	dockerId := buf.CreateString(info.DockerId)

	protocol.AppStart(buf)
	protocol.AppAddAgentLanguage(buf, lang)
	protocol.AppAddAgentVersion(buf, version)
	protocol.AppAddAppName(buf, appname)
	protocol.AppAddLicense(buf, license)
	protocol.AppAddRedirectCollector(buf, collector)
	protocol.AppAddEnvironment(buf, env)
	protocol.AppAddLabels(buf, labels)
	protocol.AppAddMetadata(buf, metadata)
	protocol.AppAddSettings(buf, settings)
	protocol.AppAddHost(buf, host)
	protocol.AppAddTraceObserverHost(buf, traceObserverHost)
	protocol.AppAddTraceObserverPort(buf, info.TraceObserverPort)

	protocol.AppAddHighSecurity(buf, info.HighSecurity)
	protocol.AppAddDockerId(buf, dockerId)

	appInfo := protocol.AppEnd(buf)

	protocol.MessageStart(buf)
	protocol.MessageAddDataType(buf, protocol.MessageBodyApp)
	protocol.MessageAddData(buf, appInfo)
	rootOffset := protocol.MessageEnd(buf)
	buf.Finish(rootOffset)
	return buf.Bytes[buf.Head():], nil
}

type Txn struct {
	RunID            string
	Name             string
	SamplingPriority newrelic.SamplingPriority
	Metrics          []metric
	Errors           []*newrelic.Error
	Trace            *newrelic.TxnTrace
	SlowSQLs         []*newrelic.SlowSQL
	AnalyticEvent    json.RawMessage
	CustomEvents     []json.RawMessage
	ErrorEvents      []json.RawMessage
	SpanEvents       []json.RawMessage
}

type metric struct {
	Name   string
	Data   [6]float64
	Scoped bool
	Forced bool
}

func (t *Txn) MarshalSpanBatchBinary(batchSize int, protoSpanBatch []byte) ([]byte, error) {
	buf := flatbuffers.NewBuilder(0)
	offset := buf.CreateByteVector(protoSpanBatch)

	protocol.SpanBatchStart(buf)
	protocol.SpanBatchAddCount(buf, uint64(batchSize))
	protocol.SpanBatchAddEncoded(buf, offset)
	dataOffset := protocol.SpanBatchEnd(buf)

	id := buf.CreateString(t.RunID)
	protocol.MessageStart(buf)
	protocol.MessageAddAgentRunId(buf, id)
	protocol.MessageAddDataType(buf, protocol.MessageBodySpanBatch)
	protocol.MessageAddData(buf, dataOffset)
	buf.Finish(protocol.MessageEnd(buf))

	return buf.Bytes[buf.Head():], nil
}

func (t *Txn) MarshalBinary() ([]byte, error) {
	buf := flatbuffers.NewBuilder(0)

	// Transaction Event
	var analyticEvent flatbuffers.UOffsetT
	if len(t.AnalyticEvent) > 0 {
		analyticEvent = protocol.EncodeEvent(buf, []byte(t.AnalyticEvent))
	}

	var (
		metrics      = encodeMetrics(buf, t.Metrics)
		errors       = encodeErrors(buf, t.Errors)
		slowSQLs     = encodeSlowSQLs(buf, t.SlowSQLs)
		customEvents = encodeEvents(buf, t.CustomEvents)
		errorEvents  = encodeErrorEvents(buf, t.ErrorEvents)
		trace        = encodeTrace(buf, t.Trace)
		spanEvents   = encodeSpanEvents(buf, t.SpanEvents)
	)

	var txnName, txnURI, syntheticsResourceID flatbuffers.UOffsetT

	if tt := t.Trace; tt != nil {
		if tt.SyntheticsResourceID != "" {
			syntheticsResourceID = buf.CreateString(tt.SyntheticsResourceID)
		}
		if tt.RequestURI != "" {
			txnURI = buf.CreateString(tt.RequestURI)
		}
	}

	txnName = buf.CreateString(t.Name)
	if txnURI == 0 {
		txnURI = buf.CreateString("<unknown>")
	}

	protocol.TransactionStart(buf)
	protocol.TransactionAddName(buf, txnName)
	protocol.TransactionAddUri(buf, txnURI)
	protocol.TransactionAddSyntheticsResourceId(buf, syntheticsResourceID)
	protocol.TransactionAddPid(buf, int32(os.Getpid()))
	protocol.TransactionAddTxnEvent(buf, analyticEvent)
	protocol.TransactionAddMetrics(buf, metrics)
	protocol.TransactionAddErrors(buf, errors)
	protocol.TransactionAddSlowSqls(buf, slowSQLs)
	protocol.TransactionAddCustomEvents(buf, customEvents)
	protocol.TransactionAddErrorEvents(buf, errorEvents)
	protocol.TransactionAddTrace(buf, trace)
	protocol.TransactionAddSpanEvents(buf, spanEvents)
	dataOffset := protocol.TransactionEnd(buf)

	id := buf.CreateString(t.RunID)
	protocol.MessageStart(buf)
	protocol.MessageAddAgentRunId(buf, id)
	protocol.MessageAddDataType(buf, protocol.MessageBodyTransaction)
	protocol.MessageAddData(buf, dataOffset)
	buf.Finish(protocol.MessageEnd(buf))
	return buf.Bytes[buf.Head():], nil
}

func encodeMetrics(b *flatbuffers.Builder, metrics []metric) flatbuffers.UOffsetT {
	if n := len(metrics); n > 0 {
		offsets := make([]flatbuffers.UOffsetT, n)
		for i := n - 1; i >= 0; i-- {
			offsets[i] = protocol.EncodeMetric(b, metrics[i].Name,
				metrics[i].Data, metrics[i].Scoped, metrics[i].Forced)
		}

		protocol.TransactionStartMetricsVector(b, n)
		for i := n - 1; i >= 0; i-- {
			b.PrependUOffsetT(offsets[i])
		}

		return b.EndVector(n)
	}
	return 0
}

func encodeErrorEvents(b *flatbuffers.Builder, events []json.RawMessage) flatbuffers.UOffsetT {
	if n := len(events); n > 0 {
		offsets := make([]flatbuffers.UOffsetT, n)
		for i := n - 1; i >= 0; i-- {
			offsets[i] = protocol.EncodeEvent(b, []byte(events[i]))
		}

		protocol.TransactionStartErrorEventsVector(b, n)
		for i := n - 1; i >= 0; i-- {
			b.PrependUOffsetT(offsets[i])
		}

		return b.EndVector(n)
	}
	return 0
}

func encodeErrors(b *flatbuffers.Builder, errors []*newrelic.Error) flatbuffers.UOffsetT {
	if n := len(errors); n > 0 {
		offsets := make([]flatbuffers.UOffsetT, n)
		for i := n - 1; i >= 0; i-- {
			offsets[i] = protocol.EncodeError(b, int32(errors[i].Priority), errors[i].Data)
		}

		protocol.TransactionStartErrorsVector(b, n)
		for i := n - 1; i >= 0; i-- {
			b.PrependUOffsetT(offsets[i])
		}

		return b.EndVector(n)
	}
	return 0
}

func encodeEvents(b *flatbuffers.Builder, events []json.RawMessage) flatbuffers.UOffsetT {
	if n := len(events); n > 0 {
		offsets := make([]flatbuffers.UOffsetT, n)
		for i := n - 1; i >= 0; i-- {
			offsets[i] = protocol.EncodeEvent(b, []byte(events[i]))
		}

		protocol.TransactionStartCustomEventsVector(b, n)
		for i := n - 1; i >= 0; i-- {
			b.PrependUOffsetT(offsets[i])
		}

		return b.EndVector(n)
	}
	return 0
}

func encodeSpanEvents(b *flatbuffers.Builder, events []json.RawMessage) flatbuffers.UOffsetT {
	if n := len(events); n > 0 {
		offsets := make([]flatbuffers.UOffsetT, n)
		for i := n - 1; i >= 0; i-- {
			offsets[i] = protocol.EncodeEvent(b, []byte(events[i]))
		}

		protocol.TransactionStartSpanEventsVector(b, n)
		for i := n - 1; i >= 0; i-- {
			b.PrependUOffsetT(offsets[i])
		}

		return b.EndVector(n)
	}
	return 0
}

func encodeSlowSQLs(b *flatbuffers.Builder, sqls []*newrelic.SlowSQL) flatbuffers.UOffsetT {
	if n := len(sqls); n > 0 {
		offsets := make([]flatbuffers.UOffsetT, n)

		for i := n - 1; i >= 0; i-- {
			offsets[i] = protocol.EncodeSlowSQL(b, uint32(sqls[i].ID),
				sqls[i].Count, sqls[i].TotalMicros, sqls[i].MinMicros,
				sqls[i].MaxMicros, sqls[i].MetricName, sqls[i].Query, sqls[i].Params)
		}

		protocol.TransactionStartSlowSqlsVector(b, n)
		for i := n - 1; i >= 0; i-- {
			b.PrependUOffsetT(offsets[i])
		}

		return b.EndVector(n)
	}
	return 0
}

func encodeTrace(b *flatbuffers.Builder, tt *newrelic.TxnTrace) flatbuffers.UOffsetT {
	if tt != nil {
		return protocol.EncodeTrace(b, tt.UnixTimestampMillis, tt.DurationMillis, tt.GUID, tt.ForcePersist, tt.Data)
	}
	return 0
}
