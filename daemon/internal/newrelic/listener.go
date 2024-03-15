//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

// listener.go contains the logic responsible for managing agent connections.
// It is responsible for:
//   * Accepting new connections
//   * Reading incoming data from connections
//   * Parsing the preamble and envelope from inbound messages
//   * Dispatching the command to the AgentDataHandler
//   * Writing any reply created by the AgentDataHandler

const (
	maxMessageSize = 2 << 20 /* 2 MB */
	msgHeaderSize  = 8
)

// MessageType identifies the encoding for a message body.
type MessageType uint32

const (
	MessageTypeRaw MessageType = iota
	MessageTypeJSON
	MessageTypeBinary
)

var byteOrder = binary.LittleEndian

// DefaultListenSocket is the default location for the agent daemon
// communication socket. Note that this should match the agent's
// NR_PHP_INI_DEFAULT_PORT value.
//
// Linux systems use the abstract socket "@newrelic" by default, MacOS and
// FreeBSD use the socket file "/tmp/.newrelic.sock".
func DefaultListenSocket() string {
	if runtime.GOOS == "linux" {
		return "@newrelic"
	} else {
		return "/tmp/.newrelic.sock"
	}
}

type Listener struct {
	listener net.Listener
}

func Listen(network, addr string) (*Listener, error) {
	// For unix sockets, ensure the socket is accessible by all.
	if network == "unix" || network == "unixpacket" {
		// The result of fchmod(3) on a socket is undefined so umask(3) is the
		// only reliable way to control the permissions on a sock file. Since
		// it is per-process, saving and restoring the umask is racey in
		// multithreaded programs. We rely on the fact that the daemon opens
		// few files and hope we don't have any other threads to race.
		defer syscall.Umask(syscall.Umask(0))
	}

	listener, err := net.Listen(network, addr)
	if err != nil {
		return nil, err
	}

	return &Listener{listener: listener}, nil
}

func (l *Listener) Close() error {
	return l.listener.Close()
}

func (l *Listener) Serve(h MessageHandler) error {
	var cooldown time.Duration

	for {
		conn, err := l.listener.Accept()
		if err != nil {
			if netErr, ok := err.(net.Error); ok && netErr.Temporary() {
				// Transient error condition, stop accepting new connections
				// for a short while and see if the condition clears. If the
				// problem persists, double the cooldown period up to a maximum
				// of 1 second.
				if cooldown == 0 {
					cooldown = 5 * time.Millisecond
				} else {
					cooldown *= 2
				}
				if max := 1 * time.Second; cooldown > max {
					cooldown = max
				}

				log.Debugf("accept error: %v, retrying in %v", err, cooldown)
				time.Sleep(cooldown)
				continue
			}
			return err
		}

		cooldown = 0
		go serve(conn, h)
	}
}

// serve reads and responds to messages from the given connection until
// an error occurs or the connection is closed.
func serve(c net.Conn, h MessageHandler) {
	clientConn := conn{}
	clientConn.rwc = c
	clientConn.handler = h
	clientConn.mw.W = c

	defer func() {
		if err := recover(); err != nil {
			log.Errorf("listener panic: %v\n%s", err, log.StackTrace())
		}

		if err := clientConn.Close(); err != nil {
			log.Debugf("listener: error closing client connection: %v", err)
		}
	}()

	clientConn.Serve()
}

type MessageHandler interface {
	HandleMessage(RawMessage) ([]byte, error)
}

// conn wraps a client connection.
type conn struct {
	rwc     net.Conn       // underlying connection
	handler MessageHandler // routes messages to the processor
	mw      MessageWriter  // writer for outgoing messages
	stats   connStats      // not implemented yet
}

type connStats struct {
	count   int // number of messages consumed
	drops   int // number of messages dropped
	errors  int // number of messages failed
	minSize int
	maxSize int
}

// Close closes the connection.
// Any blocked operations will be unblocked and return errors.
func (c *conn) Close() error {
	return c.rwc.Close()
}

// Serve pumps messages from c until EOF is reached or an error occurs.
func (c *conn) Serve() {
	for {
		msg, err := ReadMessage(c.rwc)
		if err != nil {
			if err != io.EOF {
				if err == errLegacyAgent {
					// Send the agent an empty message containing a newer protocol
					// version to cause it log a version mismatch error and then
					// close the connection.
					c.rwc.Write([]byte{'5', ' ', '0', ' ', '0', '\n', 0, 0, 0, 0})
				}
				log.Errorf("listener: closing connection: %v", err)
			}
			return
		}

		reply, perr := c.handler.HandleMessage(msg)
		if nil != perr {
			log.Warnf("listener: protocol error: %v", perr)
			// We do not close the connection here: As long
			// as the messages are delineated, there is
			// nothing to gain by making this faulty agent
			// reconnect.
		}

		if nil != reply {
			// Assume that the reply type is the same as the
			// message type.
			c.mw.Type = msg.Type
			_, err := c.mw.Write(reply)
			if nil != err {
				log.Errorf("listener: closing connection: unable to write reply of length %d: %v",
					len(reply), err)
				return
			}
		}
	}
}

func isLegacyAgent(p []byte) bool {
	// Legacy header format:
	//   [0-9] SPACE [0-9] SPACE [0] NEWLINE

	if len(p) < 6 {
		return false
	}

	// Note that bytes 5 and 6 prevent a collision between the new header
	// and the old header. `MessageType` will not contain `[0] NEWLINE`.
	switch {
	case p[0] < '0' || p[0] > '9':
		// Invalid major version.
		return false
	case p[1] != ' ':
		return false
	case p[2] < '0' || p[2] > '9':
		// Invalid minor version.
		return false
	case p[3] != ' ':
		return false
	case p[4] != '0':
		// Invalid format.
		return false
	case p[5] != '\n':
		return false
	}

	return true
}

var errLegacyAgent = errors.New("agent version is older than the newrelic-daemon, this may be due a software update - try restarting the agent")

func ReadMessage(r io.Reader) (RawMessage, error) {
	header := [msgHeaderSize]byte{}
	_, err := io.ReadFull(r, header[:])
	if nil != err {
		if err == io.EOF {
			return RawMessage{}, err
		}
		return RawMessage{}, fmt.Errorf("unable to read header: %v", err)
	}

	if isLegacyAgent(header[:]) {
		return RawMessage{}, errLegacyAgent
	}

	msgType := MessageType(byteOrder.Uint32(header[4:8]))
	dataSize := byteOrder.Uint32(header[0:4])
	if dataSize > maxMessageSize {
		// Debugging aid: guess whether the stream is out of sync.
		if msgType != MessageTypeBinary {
			log.Debugf("listener: invalid message type (%d), stream may be out of sync", msgType)
		}
		return RawMessage{}, fmt.Errorf("maximum message size exceeded, (%d > %d)",
			dataSize, maxMessageSize)
	}

	msg := make([]byte, dataSize)
	_, err = io.ReadFull(r, msg)
	if nil != err {
		return RawMessage{}, fmt.Errorf("unable to read full message: %v", err)
	}

	return RawMessage{
		Type:  msgType,
		Bytes: msg,
	}, nil
}

func OpenClientConnection(addr string) (net.Conn, error) {
	var network string
	if strings.HasPrefix(addr, "/") {
		network = "unix"
	} else {
		network = "tcp"
	}
	return net.Dial(network, addr)
}

// RawMessage contains a single message's contents: Bytes does not contain the
// message header.
type RawMessage struct {
	Type  MessageType
	Bytes []byte
}

// The minimum number of bytes (not messages!) to buffer.
const minMessageQueueSize = 1 << 16 /* 64 KB */

// A MessageWriter writes data to a stream as messages.
type MessageWriter struct {
	W      io.Writer           // underlying writer
	Type   MessageType         // message encoding
	header [msgHeaderSize]byte // scratch space for message header
}

func (mw *MessageWriter) writeHeader(length uint32) (n int, err error) {
	byteOrder.PutUint32(mw.header[0:4], uint32(length))
	byteOrder.PutUint32(mw.header[4:8], uint32(mw.Type))
	return mw.W.Write(mw.header[:])
}

// Write writes len(p) bytes from p to the underlying data stream as a
// single message. It returns the number of bytes written and any error
// encountered that caused the write to stop early. When write fails to
// write the complete message, the underlying data stream should be
// assumed to be out of sync.
func (mw *MessageWriter) Write(p []byte) (n int, err error) {
	nw, err := mw.writeHeader(uint32(len(p)))
	if nw > 0 {
		n += nw
	}

	if err == nil && len(p) > 0 {
		nw, err = mw.W.Write(p)
		if nw > 0 {
			n += nw
		}
	}

	return
}

// WriteString writes the contents of the string s to mw as a message. If
// the underlying writer implements a WriteString method, it is invoked
// directly.
func (mw *MessageWriter) WriteString(s string) (n int, err error) {
	length := uint32(len(s))

	nw, err := mw.writeHeader(length)
	if nw > 0 {
		n += nw
	}

	if err == nil && length > 0 {
		nw, err = io.WriteString(mw.W, s)
		if nw > 0 {
			n += nw
		}
	}

	return
}

func (mt MessageType) String() string {
	switch mt {
	case MessageTypeRaw:
		return "raw"
	case MessageTypeJSON:
		return "JSON"
	case MessageTypeBinary:
		return "binary"
	default:
		return "MessageType(" + strconv.Itoa(int(mt)) + ")"
	}
}
