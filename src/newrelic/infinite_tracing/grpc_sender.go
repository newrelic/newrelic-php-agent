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
	"google.golang.org/grpc/encoding"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	v1 "newrelic/infinite_tracing/com_newrelic_trace_v1"
	"newrelic/log"
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
	encoding.Codec
}

var (
	supportabilityCodeErr = "Supportability/InfiniteTracing/Span/gRPC/"
	codeStrings           = func() map[codes.Code]string {
		codeStrings := make(map[codes.Code]string, numCodes)
		for i := 0; i < numCodes; i++ {
			code := codes.Code(i)
			codeStrings[code] = strings.ToUpper(code.String())
		}
		return codeStrings
	}()
)

func (c *codec) Marshal(v interface{}) ([]byte, error) {
	if batch, ok := v.(encodedSpanBatch); ok {
		return []byte(batch), nil
	}

	return c.Codec.Marshal(v)
}

func (c *codec) Unmarshal(data []byte, v interface{}) error {
	return c.Codec.Unmarshal(data, v)
}

func (c *codec) Name() string { return c.Codec.Name() }

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
	stream, err := s.client.RecordSpanBatch(
		metadata.AppendToOutgoingContext(context.Background(),
			"license_key", s.License,
			"agent_run_token", s.RunId),
		grpc.ForceCodec(&codec{encoding.GetCodec("proto")}))

	if err != nil {
		log.Errorf("cannot establish stream to grpc endpoint: %v", err)
		return err, newSpanBatchStatusFromGrpcErr(err)
	}
	s.stream = stream

	log.Debugf("connected to grpc endpoint %s", s.Host)
	go func() {
		for {
			in, err := s.stream.Recv()

			switch err {
			case nil:
				log.Debugf("grpc endpoint messages seen: %d", in.MessagesSeen)
			case io.EOF:
				log.Debugf("received EOF from grpc endpoint")
				return
			default:
				log.Errorf("unexpected error from grpc endpoint:  %v", err)
				status := newSpanBatchStatusFromGrpcErr(err)
				if status.code == statusShutdown {
					s.responseError <- status
					return
				} else {
					status.code = statusOk
					s.responseError <- status
				}
			}
		}
	}()

	return nil, spanBatchSenderStatus{code: statusOk}
}

func (s *grpcSpanBatchSender) send(batch encodedSpanBatch) (error, spanBatchSenderStatus) {
	if err := s.stream.SendMsg(batch); err != nil {
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

func errToCodeString(err error) string {
	code := status.Code(err)
	str, ok := codeStrings[code]
	if !ok {
		str = strings.ToUpper(code.String())
	}
	return str
}
