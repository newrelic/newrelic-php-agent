//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package infinite_tracing

import (
	"fmt"
	"io"
	"net"
	"testing"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	v1 "newrelic/infinite_tracing/com_newrelic_trace_v1"
)

type testGrpcServer struct {
	server               *grpc.Server
	host                 string
	port                 uint16
	spansReceivedChan    chan *v1.SpanBatch
	metadataReceivedChan chan metadata.MD
}

func (s *testGrpcServer) RecordSpanBatch(stream v1.IngestService_RecordSpanBatchServer) error {
	md, ok := metadata.FromIncomingContext(stream.Context())
	if ok {
		s.metadataReceivedChan <- md
	}
	for {
		batch, err := stream.Recv()
		if err == io.EOF {
			return nil
		} else if nil != err {
			return err
		}
		s.spansReceivedChan <- batch
	}
}

func (s *testGrpcServer) RecordSpan(stream v1.IngestService_RecordSpanServer) error {
	for {
		_, err := stream.Recv()
		if err == io.EOF {
			return nil
		} else if nil != err {
			return err
		}
	}
}

func (ts *testGrpcServer) Close() {
	ts.server.Stop()
}

func getPortListener() (listener *net.Listener, port uint16, err error) {
	var p uint16
	for p = 10000; p < 10100; p++ {
		lis, err := net.Listen("tcp", fmt.Sprintf(":%d", p))
		if err == nil {
			return &lis, p, nil
		}
	}

	return nil, 0, err
}

func newTestObsServer(t *testing.T) *testGrpcServer {
	lis, port, err := getPortListener()

	if err != nil {
		t.Fatalf("Cannot start grpc test server: %v", err)
	}

	grpcServer := grpc.NewServer()

	s := &testGrpcServer{
		host:                 "localhost",
		port:                 port,
		server:               grpcServer,
		spansReceivedChan:    make(chan *v1.SpanBatch, 10),
		metadataReceivedChan: make(chan metadata.MD, 10),
	}

	v1.RegisterIngestServiceServer(s.server, s)

	go grpcServer.Serve(*lis)

	return s
}

func newInvalidObsServer(t *testing.T) *testGrpcServer {
	lis, port, err := getPortListener()

	if err != nil {
		t.Fatalf("Cannot start grpc test server: %v", err)
	}

	grpcServer := grpc.NewServer()

	go grpcServer.Serve(*lis)

	return &testGrpcServer{
		host:                 "localhost",
		port:                 port,
		server:               grpcServer,
		spansReceivedChan:    make(chan *v1.SpanBatch, 10),
		metadataReceivedChan: make(chan metadata.MD, 10),
	}
}

func TestInvalidHost(t *testing.T) {
	_, err := newGrpcSpanBatchSender(&Config{
		Host:   "@@@@@@@@@",
		Port:   443,
		Secure: false,
	})

	if err != nil {
		t.Errorf("grpc connect doesn't fail on invalid host")
	}
}

func TestUnsupportedProtocol(t *testing.T) {
	srv := newInvalidObsServer(t)
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	defer sender.conn.Close()

	responseError := sender.response()

	err, _ = sender.connect()
	if err != nil {
		t.Fatalf("unexpected error during connect: %v", err)
	}

	status := <-responseError

	if status.code != statusShutdown {
		t.Fatalf("expected statusShutdown on responseError")
	}
}

func TestConnectionParams(t *testing.T) {
	srv := newTestObsServer(t)
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:    srv.host,
		Port:    srv.port,
		Secure:  false,
		License: "lic",
		RunId:   "runid",
	})
	defer sender.conn.Close()

	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}

	err, _ = sender.connect()
	if err != nil {
		t.Fatalf("unexpected error during connect: %v", err)
	}

	md := <-srv.metadataReceivedChan

	expected := map[string]string{
		"agent_run_token": "runid",
		"license_key":     "lic",
	}

	for expectedKey, expectedValue := range expected {
		value, ok := md[expectedKey]
		if ok && len(value) == 1 {
			if value[0] != expectedValue {
				t.Errorf("invalid value for %s metadata: %s", expectedKey, value[0])
			}
		} else {
			t.Errorf("no value for %s metadata", expectedKey)
		}
	}
}

func TestSimpleSpan(t *testing.T) {
	srv := newTestObsServer(t)
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:              srv.host,
		Port:              srv.port,
		Secure:            false,
		RequestHeadersMap: map[string]string{"zip": "zap"},
	})
	defer sender.conn.Close()

	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}

	err, _ = sender.connect()
	if err != nil {
		t.Fatalf("unexpected error during connect: %v", err)
	}

	s := &v1.Span{
		TraceId: "trace_id",
	}
	b := &v1.SpanBatch{
		Spans: []*v1.Span{s},
	}
	bs, _ := proto.Marshal(b)

	err, _ = sender.send(encodedSpanBatch(bs))
	if err != nil {
		t.Fatalf("unexpected error during sending: %v", err)
	}

	md := <-srv.metadataReceivedChan

	expected := map[string]string{
		"zip": "zap",
	}

	for expectedKey, expectedValue := range expected {
		value, ok := md[expectedKey]
		if ok && len(value) == 1 {
			if value[0] != expectedValue {
				t.Errorf("invalid value for key %s metadata: got %s, want %s", expectedKey, value[0], expectedValue)
			}
		} else {
			t.Errorf("no value for %s metadata", expectedKey)
		}
	}

	received := <-srv.spansReceivedChan

	if len(received.Spans) != 1 {
		t.Errorf("1 span expected, received %d", len(received.Spans))
	}

	if received.Spans[0].TraceId != s.TraceId {
		t.Errorf("expected: %s\n, actual %s", received.Spans[0].TraceId, s.TraceId)
	}
}

func TestInvalidSpan(t *testing.T) {
	srv := newTestObsServer(t)
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	defer sender.conn.Close()

	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}

	responseError := sender.response()

	err, _ = sender.connect()
	if err != nil {
		t.Fatalf("unexpected error during connect: %v", err)
	}

	err, _ = sender.send(encodedSpanBatch([]byte{1, 2, 3}))
	if err != nil {
		t.Fatalf("unexpected error during sending: %v", err)
	}

	status := <-responseError

	if status.code != statusOk {
		t.Fatalf("expected statusOk on responseError")
	}
}

func TestErrToCodeString(t *testing.T) {
	// Test that no error codes have changed
	testcases := []struct {
		code   codes.Code
		expect string
	}{
		{code: 0, expect: "OK"},
		{code: 1, expect: "CANCELED"},
		{code: 2, expect: "UNKNOWN"},
		{code: 3, expect: "INVALIDARGUMENT"},
		{code: 4, expect: "DEADLINEEXCEEDED"},
		{code: 5, expect: "NOTFOUND"},
		{code: 6, expect: "ALREADYEXISTS"},
		{code: 7, expect: "PERMISSIONDENIED"},
		{code: 8, expect: "RESOURCEEXHAUSTED"},
		{code: 9, expect: "FAILEDPRECONDITION"},
		{code: 10, expect: "ABORTED"},
		{code: 11, expect: "OUTOFRANGE"},
		{code: 12, expect: "UNIMPLEMENTED"},
		{code: 13, expect: "INTERNAL"},
		{code: 14, expect: "UNAVAILABLE"},
		{code: 15, expect: "DATALOSS"},
		{code: 16, expect: "UNAUTHENTICATED"},
		// test one more than the number of codes supported by grpc so we
		// can detect when a new code is added
		{code: 17, expect: "CODE(17)"},
	}

	for _, test := range testcases {
		t.Run(test.expect, func(t *testing.T) {
			err := status.Error(test.code, "error")
			actual := errToCodeString(err)
			if actual != test.expect {
				t.Errorf("wrong error returned: actual=%s expected=%s",
					actual, test.expect)
			}
		})
	}
}
