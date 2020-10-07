//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package proto_testdata

import (
	"google.golang.org/protobuf/proto"
	
	v1 "newrelic/infinite_tracing/com_newrelic_trace_v1"
)

type spanEvent struct {
	TraceID         string
	Intrinsics      map[string]interface{}
	UserAttributes  map[string]interface{}
	AgentAttributes map[string]interface{}
}

var (
	sampleSpanEvent = spanEvent{
		TraceID: "traceid",
		Intrinsics: map[string]interface{}{
			"type":          "Span",
			"traceId":       "traceid",
			"guid":          "guid",
			"parentId":      "parentid",
			"transactionId": "txnid",
			"sampled":       true,
			"priority":      0.500000,
			"timestamp":     10,
			"duration":      0.00101,
			"name":          "name",
			"category":      "http",
		},
		UserAttributes: map[string]interface{}{},
		AgentAttributes: map[string]interface{}{},
	}
)

func obsvString(s string) *v1.AttributeValue {
	return &v1.AttributeValue{Value: &v1.AttributeValue_StringValue{StringValue: s}}
}

func obsvBool(b bool) *v1.AttributeValue {
	return &v1.AttributeValue{Value: &v1.AttributeValue_BoolValue{BoolValue: b}}
}

func obsvInt(x int64) *v1.AttributeValue {
	return &v1.AttributeValue{Value: &v1.AttributeValue_IntValue{IntValue: x}}
}

func obsvDouble(x float64) *v1.AttributeValue {
	return &v1.AttributeValue{Value: &v1.AttributeValue_DoubleValue{DoubleValue: x}}
}

func MarshalSpanBatch(batchSize uint) ([]byte, error) {
	spanBatch := &v1.SpanBatch{
		Spans: []*v1.Span{},
	}
	
	span := transformSpanEvent(&sampleSpanEvent)

	for i := batchSize; i > 0; i-- {
		spanBatch.Spans = append(spanBatch.Spans, span)
	}

	buf, err := proto.Marshal(spanBatch)
	if err != nil {
		return nil, err
	}

	return buf, err
}

func transformSpanEvent(se *spanEvent) *v1.Span {
	span := &v1.Span{
		TraceId:         se.TraceID,
		Intrinsics:      make(map[string]*v1.AttributeValue),
		UserAttributes:  make(map[string]*v1.AttributeValue),
		AgentAttributes: make(map[string]*v1.AttributeValue),
	}

	span.Intrinsics["type"] = obsvString(se.Intrinsics["type"].(string))
	span.Intrinsics["traceId"] = obsvString(se.Intrinsics["traceId"].(string))
	span.Intrinsics["guid"] = obsvString(se.Intrinsics["guid"].(string))
	span.Intrinsics["parentId"] = obsvString(se.Intrinsics["parentId"].(string))
	span.Intrinsics["transactionId"] = obsvString(se.Intrinsics["transactionId"].(string))
	span.Intrinsics["sampled"] = obsvBool(se.Intrinsics["sampled"].(bool))
	span.Intrinsics["priority"] = obsvDouble(se.Intrinsics["priority"].(float64))
	span.Intrinsics["timestamp"] = obsvInt(int64(se.Intrinsics["timestamp"].(int)))
	span.Intrinsics["duration"] = obsvDouble(se.Intrinsics["duration"].(float64))
	span.Intrinsics["name"] = obsvString(se.Intrinsics["name"].(string))
	span.Intrinsics["category"] = obsvString(se.Intrinsics["category"].(string))

	return span
}
