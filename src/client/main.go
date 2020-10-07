//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"

	"flatbuffersdata"
	"newrelic"
	"newrelic/protocol"

	"github.com/google/flatbuffers/go"
)

var (
	addr       = flag.String("addr", newrelic.DefaultListenSocket(), "daemon address")
	agentRunID = flag.String("run", "", "agent run id")
)

func main() {
	flag.Usage = func() {
		fmt.Fprint(os.Stderr, "Usage: client [-addr ADDRESS] [-run AGENT_RUN_ID] [FILE...]\n\n")
		flag.PrintDefaults()
	}
	flag.Parse()

	conn, err := newrelic.OpenClientConnection(*addr)
	if nil != err {
		fatal(err)
	}
	defer conn.Close()

	if flag.NArg() == 0 {
		fmt.Println(`please provide argument: "appinfo" or "txndata"`)
	} else {
		var n int

		cmd := flag.Arg(0)
		switch cmd {
		case "appinfo":
			n, err = doAppInfo(conn)
		case "txndata":
			mw := &newrelic.MessageWriter{W: conn, Type: newrelic.MessageTypeBinary}
			n, err = doTransaction(mw)
		default:
			fmt.Println(`please provide argument: "appinfo" or "txndata"`)
		}
		fmt.Fprintf(os.Stderr, "%s: %d bytes written\n", cmd, n)
	}

	if err != nil {
		fatal(err)
	}
}

func doAppInfo(rw io.ReadWriter) (n int, err error) {
	query, err := flatbuffersdata.MarshalAppInfo(&flatbuffersdata.SampleAppInfo)
	if err != nil {
		return 0, err
	}

	mw := newrelic.MessageWriter{W: rw, Type: newrelic.MessageTypeBinary}
	n, err = mw.Write(query)
	if err != nil {
		return
	}

	data, err := readMessage(rw)
	if err != nil {
		return n, err
	}

	msg := protocol.GetRootAsMessage(data, 0)
	if msg.DataType() != protocol.MessageBodyAppReply {
		return n, errors.New("unexpected reply type")
	}

	var tbl flatbuffers.Table
	var reply protocol.AppReply

	if !msg.Data(&tbl) {
		return n, errors.New("reply missing body")
	}

	reply.Init(tbl.Bytes, tbl.Pos)
	fmt.Printf("reply:\n%s\n", reply.ConnectReply())
	return n, nil
}

func doTransaction(w io.Writer) (n int, err error) {
	txn := flatbuffersdata.SampleTxn
	txn.RunID = *agentRunID
	msg, err := txn.MarshalBinary()
	if err != nil {
		return 0, err
	}
	return w.Write(msg)
}

func readMessage(r io.Reader) ([]byte, error) {
	b := make([]byte, 8)
	if _, err := io.ReadFull(r, b); err != nil {
		if err == io.EOF {
			return nil, err
		}
		return nil, fmt.Errorf("unable to read preamble: %v", err)
	}

	length := binary.LittleEndian.Uint32(b[0:4])

	body := make([]byte, length)
	if _, err := io.ReadFull(r, body); err != nil {
		return nil, fmt.Errorf("unable to read full message: %v", err)
	}
	return body, nil
}

func fatal(e error) {
	fmt.Fprintln(os.Stderr, e)
	os.Exit(1)
}
