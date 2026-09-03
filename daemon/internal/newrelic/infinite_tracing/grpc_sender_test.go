//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package infinite_tracing

import (
	"context"
	"fmt"
	"io"
	"net"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	v1 "github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1"
)

type testGrpcServer struct {
	server               *grpc.Server
	host                 string
	port                 uint16
	spansReceivedChan    chan *v1.SpanBatch
	metadataReceivedChan chan metadata.MD
	closeAfterOneMessage bool
	ackThenWait          bool
	ackStreamCalls       int32
	firstAckSent         chan struct{}
	proceedWithSecondAck chan struct{}
}

func (s *testGrpcServer) RecordSpanBatch(stream v1.IngestService_RecordSpanBatchServer) error {
	md, ok := metadata.FromIncomingContext(stream.Context())
	if ok {
		s.metadataReceivedChan <- md
	}
	if s.ackThenWait {
		return s.recordSpanBatchAckThenWait(stream)
	}
	for {
		batch, err := stream.Recv()
		if err == io.EOF {
			return nil
		} else if nil != err {
			return err
		}
		s.spansReceivedChan <- batch
		if s.closeAfterOneMessage {
			return nil
		}
	}
}

// recordSpanBatchAckThenWait handles the FIRST RecordSpanBatch call by
// sending one ack, signalling the test, then waiting to be told to send a
// second ack. Every later call (a second connect() on the same sender)
// just idles until the client disconnects, so it never interferes.
func (s *testGrpcServer) recordSpanBatchAckThenWait(stream v1.IngestService_RecordSpanBatchServer) error {
	if atomic.AddInt32(&s.ackStreamCalls, 1) == 1 {
		if err := stream.Send(&v1.RecordStatus{MessagesSeen: 1}); err != nil {
			return err
		}
		close(s.firstAckSent)
		<-s.proceedWithSecondAck
		stream.Send(&v1.RecordStatus{MessagesSeen: 2})
		return nil
	}
	<-stream.Context().Done()
	return nil
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

func newAckThenWaitObsServer(t *testing.T) *testGrpcServer {
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
		ackThenWait:          true,
		firstAckSent:         make(chan struct{}),
		proceedWithSecondAck: make(chan struct{}),
	}

	v1.RegisterIngestServiceServer(s.server, s)

	go grpcServer.Serve(*lis)

	return s
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

	if status.code != statusRestart {
		t.Fatalf("expected statusRestart on responseError, got %v", status.code)
	}
	if status.metric != "Supportability/InfiniteTracing/Span/gRPC/INTERNAL" {
		t.Fatalf("expected the INTERNAL supportability metric, got %q", status.metric)
	}

	// The receive goroutine must push exactly once and return - not loop
	// back into Recv() and refill the channel. The drain at the top of
	// connect() depends on every exit path behaving this way.
	select {
	case extra := <-responseError:
		t.Fatalf("receive goroutine kept pushing after a genuine error: %v", extra)
	case <-time.After(100 * time.Millisecond):
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

func TestServerClosesWithOK(t *testing.T) {
	srv := newTestObsServer(t)
	srv.closeAfterOneMessage = true
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}
	defer sender.conn.Close()

	responseError := sender.response()

	err, _ = sender.connect()
	if err != nil {
		t.Fatalf("unexpected error during connect: %v", err)
	}

	s := &v1.Span{TraceId: "trace_id"}
	b := &v1.SpanBatch{Spans: []*v1.Span{s}}
	bs, _ := proto.Marshal(b)

	err, _ = sender.send(encodedSpanBatch(bs))
	if err != nil {
		t.Fatalf("unexpected error during sending: %v", err)
	}

	select {
	case status := <-responseError:
		if status.code != statusImmediateRestart {
			t.Fatalf("expected statusImmediateRestart on responseError, got %v", status.code)
		}
		if status.metric != "" {
			t.Fatalf("expected no supportability metric for a graceful close, got %q", status.metric)
		}
	case <-time.After(1 * time.Second):
		t.Fatalf("timed out waiting for responseError after OK close")
	}
}

func TestSendAfterServerOkCloseGetsCorrectStatus(t *testing.T) {
	srv := newTestObsServer(t)
	srv.closeAfterOneMessage = true
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}
	defer sender.conn.Close()

	err, _ = sender.connect()
	if err != nil {
		t.Fatalf("unexpected error during connect: %v", err)
	}

	s := &v1.Span{TraceId: "trace_id"}
	b := &v1.SpanBatch{Spans: []*v1.Span{s}}
	bs, _ := proto.Marshal(b)

	// First send triggers the server's graceful OK close after one message.
	if err, _ := sender.send(encodedSpanBatch(bs)); err != nil {
		t.Fatalf("unexpected error during first send: %v", err)
	}

	// The transport doesn't guarantee exactly when a subsequent SendMsg
	// discovers the closed stream, so poll rather than assume the first
	// retry hits it.
	deadline := time.Now().Add(2 * time.Second)
	var sendErr error
	var status spanBatchSenderStatus
	for time.Now().Before(deadline) {
		sendErr, status = sender.send(encodedSpanBatch(bs))
		if sendErr != nil {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	if sendErr == nil {
		t.Fatalf("expected send to eventually observe the closed stream")
	}
	if status.code != statusImmediateRestart {
		t.Fatalf("expected statusImmediateRestart after OK close, got %v", status.code)
	}
}

func TestConnectDrainsStaleResponseError(t *testing.T) {
	srv := newTestObsServer(t)
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}
	defer sender.conn.Close()

	if err, _ := sender.connect(); err != nil {
		t.Fatalf("unexpected error during first connect: %v", err)
	}

	// Simulate a stale push left over from a previous generation: something
	// the old generation's receive goroutine pushed after send()'s grace
	// period had already timed out and moved on without consuming it.
	sender.responseError <- spanBatchSenderStatus{code: statusRestart, metric: "stale"}

	if err, _ := sender.connect(); err != nil {
		t.Fatalf("unexpected error during second connect: %v", err)
	}

	// The server never closes or errors (closeAfterOneMessage is unset), so
	// the new generation's own goroutine has no reason to push anything by
	// the time connect() returns - this check is deterministic, not a race
	// against the new goroutine.
	select {
	case status := <-sender.responseError:
		t.Fatalf("expected the stale status to have been drained, got %v", status)
	default:
	}
}

// guardedRecordSpanBatchClient wraps a real stream and flags it if Recv()
// is ever called by more than one goroutine at a time - the exact
// is ever called by more than one goroutine at a time to validate 
// stream generation does not share streams between goroutines.
type guardedRecordSpanBatchClient struct {
	v1.IngestService_RecordSpanBatchClient
	recvInFlight int32
	violated     *int32
}

func (g *guardedRecordSpanBatchClient) Recv() (*v1.RecordStatus, error) {
	if !atomic.CompareAndSwapInt32(&g.recvInFlight, 0, 1) {
		atomic.StoreInt32(g.violated, 1)
	}
	defer atomic.StoreInt32(&g.recvInFlight, 0)
	return g.IngestService_RecordSpanBatchClient.Recv()
}

// guardedIngestServiceClient wraps every stream RecordSpanBatch returns in
// a guardedRecordSpanBatchClient, sharing one violated flag across all of
// them.
type guardedIngestServiceClient struct {
	v1.IngestServiceClient
	violated *int32
}

func (g *guardedIngestServiceClient) RecordSpanBatch(ctx context.Context, opts ...grpc.CallOption) (v1.IngestService_RecordSpanBatchClient, error) {
	stream, err := g.IngestServiceClient.RecordSpanBatch(ctx, opts...)
	if err != nil {
		return nil, err
	}
	return &guardedRecordSpanBatchClient{IngestService_RecordSpanBatchClient: stream, violated: g.violated}, nil
}

func TestConcurrentRecvOnReassignedStream(t *testing.T) {
	srv := newAckThenWaitObsServer(t)
	defer srv.Close()

	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   srv.host,
		Port:   srv.port,
		Secure: false,
	})
	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}
	defer sender.conn.Close()

	var violated int32
	sender.client = &guardedIngestServiceClient{IngestServiceClient: sender.client, violated: &violated}

	// Generation 1: the receive goroutine takes case nil on the first ack
	// and loops back into Recv(), where it blocks waiting for the second.
	if err, _ := sender.connect(); err != nil {
		t.Fatalf("unexpected error during first connect: %v", err)
	}

	select {
	case <-srv.firstAckSent:
	case <-time.After(1 * time.Second):
		t.Fatalf("timed out waiting for the first ack to be sent")
	}

	// Give the receive goroutine a moment to process the ack and re-enter
	// Recv() before the field it reads gets reassigned under it.
	time.Sleep(50 * time.Millisecond)

	// Generation 2: reconnect on the SAME sender - simulating a
	// send-failure-triggered reconnect racing the still-looping
	// generation-1 goroutine.
	if err, _ := sender.connect(); err != nil {
		t.Fatalf("unexpected error during second connect: %v", err)
	}

	// Release the second ack on stream 1. If defect 4 is present, the
	// generation-1 goroutine reads the reassigned s.stream field and calls
	// Recv() on stream 2, concurrently with generation 2's own goroutine.
	close(srv.proceedWithSecondAck)

	time.Sleep(200 * time.Millisecond)

	if atomic.LoadInt32(&violated) != 0 {
		t.Fatalf("concurrent Recv() calls observed on a reassigned stream")
	}
}

// eofOnSendStream stubs just enough of
// v1.IngestService_RecordSpanBatchClient to make SendMsg return io.EOF.
// Every other method is left to panic if ever called - send()'s fallback
// path under test never reaches them, and no receive goroutine is started
// against this stream (it's assigned directly to s.stream, bypassing
// connect()), so there is nothing to push onto s.responseError.
type eofOnSendStream struct {
	v1.IngestService_RecordSpanBatchClient
}

func (e *eofOnSendStream) SendMsg(m any) error {
	return io.EOF
}

// TestSendEofGracePeriodTimeout exercises send()'s fallback branch: when
// SendMsg returns io.EOF and nothing arrives on s.responseError within
// sendEofStatusGracePeriod, send() must fall through to classifying the
// bare io.EOF itself rather than hang or return early.
func TestSendEofGracePeriodTimeout(t *testing.T) {
	sender, err := newGrpcSpanBatchSender(&Config{
		Host:   "localhost",
		Port:   10999,
		Secure: false,
	})
	if err != nil {
		t.Fatalf("error initializing sender: %v", err)
	}
	defer sender.conn.Close()

	sender.stream = &eofOnSendStream{}

	start := time.Now()
	sendErr, status := sender.send(encodedSpanBatch([]byte{1, 2, 3}))
	elapsed := time.Since(start)

	if elapsed < 900*time.Millisecond {
		t.Fatalf("send() returned after %v, expected it to wait out the ~%v grace period", elapsed, sendEofStatusGracePeriod)
	}
	if sendErr != io.EOF {
		t.Fatalf("expected send() to return io.EOF, got %v", sendErr)
	}
	if status.code != statusRestart {
		t.Fatalf("expected statusRestart, got %v", status.code)
	}
	if status.metric != "Supportability/InfiniteTracing/Span/gRPC/UNKNOWN" {
		t.Fatalf("expected the UNKNOWN supportability metric, got %q", status.metric)
	}
}
