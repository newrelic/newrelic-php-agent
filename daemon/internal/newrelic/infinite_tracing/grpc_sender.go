//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package infinite_tracing

import (
	"context"
	"crypto/tls"
	"fmt"
	"io"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/backoff"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials"
	codecproto "google.golang.org/grpc/encoding/proto"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	v1 "github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

type grpcSpanBatchSender struct {
	conn   *grpc.ClientConn
	client v1.IngestServiceClient
	stream v1.IngestService_RecordSpanBatchClient

	responseError chan spanBatchSenderStatus

	Config
}

// Implement a custom codec that can just send encoded spans as they are.
type codec struct {
}

const (
	licenseMetadataKey = "license_key"
	runIDMetadataKey   = "agent_run_token"
)

var (
	supportabilityCodeErr = "Supportability/InfiniteTracing/Span/gRPC/"
	codeStrings           = func() map[codes.Code]string {
		codeStrings := make(map[codes.Code]string, numCodes)
		for i := range numCodes {
			code := codes.Code(i)
			codeStrings[code] = strings.ToUpper(code.String())
		}
		return codeStrings
	}()
)

func (c *codec) Marshal(v any) ([]byte, error) {
	if batch, ok := v.(encodedSpanBatch); ok {
		return []byte(batch), nil
	}

	// Use the default proto Marshal
	return proto.Marshal(v.(proto.Message))
}

func (c *codec) Unmarshal(data []byte, v any) error {
	// Use default proto unmarshal
	return proto.Unmarshal(data, v.(proto.Message))
}

func (c *codec) Name() string { return codecproto.Name }

func newGrpcSpanBatchSender(cfg *Config) (*grpcSpanBatchSender, error) {
	var cred grpc.DialOption

	if cfg.Secure {
		cred = grpc.WithTransportCredentials(credentials.NewTLS(&tls.Config{}))
	} else {
		cred = grpc.WithInsecure()
	}

	connectParams := grpc.ConnectParams{
		Backoff: backoff.Config{
			BaseDelay:  15 * time.Second,
			Multiplier: 2,
			MaxDelay:   300 * time.Second,
		},
	}

	conn, err := grpc.Dial(
		fmt.Sprintf("%s:%d", cfg.Host, cfg.Port),
		cred,
		grpc.WithConnectParams(connectParams),
	)

	if nil != err {
		log.Errorf("unable to dial to grpc endpoint %s:%d: %v", cfg.Host, cfg.Port, err.Error())
		return nil, err
	}

	return &grpcSpanBatchSender{
		conn:          conn,
		client:        v1.NewIngestServiceClient(conn),
		responseError: make(chan spanBatchSenderStatus, 10),
		Config:        *cfg,
	}, nil
}

func (s *grpcSpanBatchSender) clone() (spanBatchSender, error) {
	return newGrpcSpanBatchSender(&s.Config)
}

func (s *grpcSpanBatchSender) connect() (error, spanBatchSenderStatus) {
	// Drain any status left over from a previous stream generation.
	// s.responseError is never recreated across statusRestart/
	// statusImmediateRestart reconnects (only clone(), on statusReconnect,
	// gets a fresh one), and the new generation's goroutine doesn't exist
	// yet, so anything still buffered here can only be a leftover push.
	// That leftover is guaranteed to come from an already-terminated
	// goroutine only because of a caller-side precondition, not anything
	// intrinsic to connect(): the only caller, doStreaming() in
	// trace_observer.go, calls connect() only after the previous stream
	// has already terminated (a send error, a responseError push, or
	// shutdown). A caller that reconnects while a prior stream is still
	// live could still leave a push arriving after this drain -
	// TestConcurrentRecvOnReassignedStream forces exactly that
	// interleaving to prove stream-binding is what makes this
	// safe in practice.
	for {
		select {
		case <-s.responseError:
			continue
		default:
		}
		break
	}

	md := newMetadata(s.RunId, s.License, s.RequestHeadersMap)
	ctx := metadata.NewOutgoingContext(context.Background(), md)

	stream, err := s.client.RecordSpanBatch(
		ctx,
		grpc.ForceCodec(&codec{}))

	if err != nil {
		log.Errorf("cannot establish stream to grpc endpoint: %v", err)
		return err, newSpanBatchStatusFromGrpcErr(err)
	}
	s.stream = stream

	log.Debugf("connected to grpc endpoint %s", s.Host)
	// stream is taken as a parameter (not a closure over the local above)
	// so the binding is structural: nothing inside this goroutine can
	// accidentally read s.stream, which is reassigned on every reconnect.
	go func(stream v1.IngestService_RecordSpanBatchClient) {
		for {
			in, err := stream.Recv()

			switch err {
			case nil:
				log.Debugf("grpc endpoint messages seen: %d", in.MessagesSeen)
			case io.EOF:
				log.Debugf("received EOF from grpc endpoint")
				s.responseError <- spanBatchSenderStatus{code: statusImmediateRestart}
				return
			default:
				log.Errorf("unexpected error from grpc endpoint:  %v", err)
				s.responseError <- newSpanBatchStatusFromGrpcErr(err)
				return
			}
		}
	}(stream)

	return nil, spanBatchSenderStatus{code: statusOk}
}

// sendEofStatusGracePeriod bounds how long send() waits for the receive
// goroutine's classification after an EOF from SendMsg. Not tuned to any
// measured EOF-detection latency - chosen defensively, well above what a
// healthy local transport should ever take, so the common case resolves in
// well under a millisecond and this only matters in already-degraded
// conditions.
const sendEofStatusGracePeriod = 1 * time.Second

func (s *grpcSpanBatchSender) send(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
	if err := s.stream.SendMsg(batch); err != nil {
		if err == io.EOF {
			select {
			case status := <-s.responseError:
				return err, status
			case <-time.After(sendEofStatusGracePeriod):
				log.Errorf("timed out waiting for grpc stream status after EOF from SendMsg")
			}
		}
		return err, newSpanBatchStatusFromGrpcErr(err)
	}
	return nil, spanBatchSenderStatus{code: statusOk}
}

func (s *grpcSpanBatchSender) response() chan spanBatchSenderStatus {
	return s.responseError
}

func (s *grpcSpanBatchSender) shutdown() {
	// Grant some time to send pending spans.
	time.Sleep(500 * time.Millisecond)
	s.conn.Close()
}

// This converts gRPC error codes to a status code that triggers behavior
// according to the spec.
func newSpanBatchStatusFromGrpcErr(err error) spanBatchSenderStatus {
	code := statusRestart

	switch status.Code(err) {
	case codes.Unimplemented:
		code = statusShutdown
	case codes.OK:
		code = statusImmediateRestart
	case codes.FailedPrecondition:
		code = statusReconnect
	}

	return spanBatchSenderStatus{
		code:   code,
		metric: supportabilityCodeErr + errToCodeString(err),
	}
}

// newMetadata creates a grpc metadata with proper keys and values for use when
// connecting to RecordSpan.
func newMetadata(runID string, license string, requestHeadersMap map[string]string) metadata.MD {
	md := metadata.New(requestHeadersMap)
	md.Set(licenseMetadataKey, license)
	md.Set(runIDMetadataKey, runID)
	return md
}

func errToCodeString(err error) string {
	code := status.Code(err)
	str, ok := codeStrings[code]
	if !ok {
		str = strings.ToUpper(code.String())
	}
	return str
}
