//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"io"
	"runtime"
	"testing"
)

func TestDefaultListenSocket(t *testing.T) {
	want := "@newrelic"

	if runtime.GOOS != "linux" {
		want = "/tmp/.newrelic.sock"
	}

	actual := DefaultListenSocket()

	if want != actual {
		t.Errorf("invalid default socket, want = %v got = %v", want, actual)
	}

}

func TestWriteHeader(t *testing.T) {
	buf := bytes.Buffer{}
	mw := MessageWriter{W: &buf, Type: MessageType(0x89ABCDEF)}

	mw.writeHeader(0x01234567)

	want := []byte{0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89}
	if !bytes.Equal(want, buf.Bytes()) {
		t.Errorf("invalid header, want = %v got = %v", want, buf.Bytes())
	}
}

func TestWriteMessage(t *testing.T) {
	in, _ := hex.DecodeString("DEADBEEF")
	buf := bytes.Buffer{}

	mw := MessageWriter{W: &buf, Type: MessageType(0x01234567)}
	n, err := mw.Write(in)

	if n != 12 || err != nil {
		t.Fatalf("Write(%q) = (%d, %q); want (%d, nil)", in, n, err.Error(), 12)
	}

	want := []byte{
		0x04, 0x00, 0x00, 0x00, // length
		0x67, 0x45, 0x23, 0x01, // format
		0xDE, 0xAD, 0xBE, 0xEF} // data

	if !bytes.Equal(want, buf.Bytes()) {
		t.Errorf("garbled message:\nwant=%v\n got=%v", want, buf.Bytes())
	}
}

func TestWriteEmptyMessage(t *testing.T) {
	buf := bytes.Buffer{}

	mw := MessageWriter{W: &buf, Type: MessageType(0x01234567)}
	n, err := mw.Write([]byte{})

	if n != 8 || err != nil {
		t.Fatalf("Write([]byte{}) = (%d, %q); want (%d, nil)", n, err.Error(), 8)
	}

	want := []byte{
		0x00, 0x00, 0x00, 0x00, // length
		0x67, 0x45, 0x23, 0x01} // format

	if !bytes.Equal(want, buf.Bytes()) {
		t.Errorf("garbled message:\nwant=%v\n got=%v", want, buf.Bytes())
	}
}

func TestWriteString(t *testing.T) {
	in := "testing"
	buf := bytes.Buffer{}

	mw := MessageWriter{W: &buf, Type: MessageTypeJSON}
	n, err := mw.WriteString(in)

	if n != 15 || err != nil {
		t.Fatalf("WriteString(%q) = (%d, %q); want (%d, nil)", in, n, err.Error(), 15)
	}

	le := binary.LittleEndian

	if got := le.Uint32(buf.Next(4)); got != uint32(len(in)) {
		t.Errorf("incorrect length: want=%d got=%d", len(in), got)
	}

	if got := MessageType(le.Uint32(buf.Next(4))); got != MessageTypeJSON {
		t.Errorf("wrong message type: want=JSON got=%s", got)
	}

	if got := buf.String(); got != in {
		t.Errorf("garbled message: want=%q got=%q", in, got)
	}
}

func TestReadWriteMessages(t *testing.T) {
	msgs := []string{"one", "", "two", "", "three"}

	buf := bytes.Buffer{}
	mw := MessageWriter{W: &buf, Type: MessageTypeRaw}
	// mq := messageQueue{r: &buf}

	for _, s := range msgs {
		if _, err := mw.WriteString(s); err != nil {
			t.Fatalf("WriteString(%q) = %q", s, err.Error())
		}
	}

	i := 0
	for i < len(msgs) {
		msg, err := ReadMessage(&buf)
		if err != nil {
			t.Fatal(err)
		}

		if got := msg.Type; got != mw.Type {
			t.Errorf("wrong message encoding: want=%v got=%v", mw.Type, got)
		}

		if got := string(msg.Bytes); msgs[i] != got {
			t.Errorf("wrong message body: want=%q got=%q", msgs[i], got)
		}

		i++
	}

	_, err := ReadMessage(&buf)
	if err != io.EOF {
		t.Fatal(err)
	}
}

func TestLegacyProtocolDetection(t *testing.T) {
	var testCases = []struct {
		in   []byte
		want bool
	}{
		{[]byte{'4', ' ', '9', ' ', '0', '\n'}, true},
		{[]byte{'a', ' ', '9', ' ', '0', '\n'}, false},
		{[]byte{'4', 'a', '9', ' ', '0', '\n'}, false},
		{[]byte{'4', ' ', 'a', ' ', '0', '\n'}, false},
		{[]byte{'4', ' ', '9', 'a', '0', '\n'}, false},
		{[]byte{'4', ' ', '9', ' ', 'a', '\n'}, false},
		{[]byte{'4', ' ', '9', ' ', '0', '\r'}, false},
		{[]byte{'4', ' ', '9', ' ', '0'}, false},
	}

	for _, tt := range testCases {
		if got := isLegacyAgent(tt.in); got != tt.want {
			t.Errorf("isLegacyAgent(%v) = %t, want %t", tt.in, got, tt.want)
		}
	}

	msg := make([]byte, 0, msgHeaderSize)
	msg = append(msg, '4', ' ', '9', ' ', '0', '\n')
	for len(msg) < msgHeaderSize {
		msg = append(msg, 0)
	}

	_, err := ReadMessage(bytes.NewReader(msg))
	if err == nil {
		t.Error("ReadMessage failed to detect legacy message as invalid.")
	} else if err != errLegacyAgent {
		t.Error("ReadMessage failed to detect legacy header:", err)
	}
}
