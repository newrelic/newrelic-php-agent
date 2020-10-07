//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package valgrind

import (
	"bytes"
	"fmt"
	"io"
	"net"
	"os/exec"
	"strconv"
)

// Report summarizes the output produced by Valgrind.
type Report struct {
	Pid         int
	Ppid        int
	Tool        string
	Errors      []*ErrorInfo
	ErrorCounts map[string]int
}

// ErrorInfo represents an error detected by Valgrind.
type ErrorInfo struct {
	Unique string
	Kind   string
	What   string
	Stack  []*StackFrame
	Aux    []*AuxInfo
}

// AuxInfo provides additional information regarding the cause of an error.
// For example, when Valgrind detects a double free the auxillary
// information will identify where the memory was previously freed.
type AuxInfo struct {
	What  string
	Stack []*StackFrame
}

// StackFrame represents a stack frame as identified (traced) by Valgrind.
type StackFrame struct {
	IP     string // instruction pointer
	Object string // name of the shared object or executable

	// The following fields are populated by Valgrind when debugging info
	// is available, and Valgrind was able to map the instruction pointer
	// to a function or symbol defined within Object.

	Function string // name of the function or symbol corresponding to IP
	Location string // source location where Function is defined
}

func (r *Report) MarshalText() ([]byte, error) {
	buf := bytes.Buffer{}
	printer := textPrinter{
		w: &prefixWriter{
			w:      &buf,
			prefix: []byte("==" + strconv.Itoa(r.Pid) + "== "),
		},
	}

	for i, x := range r.Errors {
		printer.PrintError(x)
		if i < len(r.Errors)-1 {
			printer.writeln("")
		}
	}

	if printer.Err != nil {
		return nil, printer.Err
	}

	return buf.Bytes(), nil
}

type textPrinter struct {
	w   io.Writer
	Err error
}

func (tp *textPrinter) PrintError(info *ErrorInfo) {
	tp.writeln(info.What)
	tp.PrintStack(info.Stack)
	for _, aux := range info.Aux {
		tp.writeln(aux.What)
		tp.PrintStack(aux.Stack)
	}
}

func (tp *textPrinter) PrintStack(frames []*StackFrame) {
	for i, x := range frames {
		tp.PrintFrame(i, x)
	}
}

func (tp *textPrinter) PrintFrame(depth int, frame *StackFrame) {
	lead := "   by"
	if depth == 0 {
		lead = "   at"
	}

	if frame.Location != "" {
		tp.writef("%s %s: %s (%s)\n", lead, frame.IP, frame.Function, frame.Location)
	} else {
		tp.writef("%s %s: %s (in %s)\n", lead, frame.IP, frame.Function, frame.Object)
	}
}

func (tp *textPrinter) write(p []byte) {
	if tp.Err == nil {
		_, tp.Err = tp.w.Write(p)
	}
}

func (tp *textPrinter) writeln(s string) {
	if tp.Err == nil {
		_, tp.Err = tp.w.Write([]byte(s))
		if tp.Err == nil {
			_, tp.Err = tp.w.Write([]byte{'\n'})
		}
	}
}

func (tp *textPrinter) writef(format string, arg ...interface{}) {
	if tp.Err == nil {
		_, tp.Err = fmt.Fprintf(tp.w, format, arg...)
	}
}

// prefixWriter prepends a given prefix to each line written to its
// underlying writer.
type prefixWriter struct {
	w      io.Writer
	prefix []byte

	// continuation indicates whether the writer is currently positioned within
	// an (as yet) unterminated line. This is used to ensure the prefix is
	// printed exactly once at the start of each line.
	continuation bool
}

func (pw *prefixWriter) Write(p []byte) (n int, err error) {
	var written int

	i := bytes.IndexByte(p, '\n')
	for i != -1 {
		if !pw.continuation {
			// p[:i+1] is a whole line
			written, err = pw.w.Write(pw.prefix)
			n += written
			if err != nil {
				return n, err
			}
		}

		pw.continuation = false
		written, err = pw.w.Write(p[:i+1])
		n += written
		if err != nil {
			return n, err
		}

		p = p[i+1:]
		i = bytes.IndexByte(p, '\n')
	}

	// Remaining bytes belong to a line whose terminator has not been seen yet.
	if len(p) > 0 {
		if !pw.continuation {
			// p is the head of a new line.
			written, err = pw.w.Write(pw.prefix)
			n += written
			if err != nil {
				return n, err
			}
		}
		pw.continuation = true
		written, err = pw.w.Write(p)
		n += written
	}

	return n, err
}

// DefaultPort is the default port for valgrind commentary sent to a socket.
const DefaultPort int = 1500

// Listener is a TCP network listener for commentary from valgrind.
type Listener struct {
	*net.TCPListener
}

func Listen(network, addr string) (*Listener, error) {
	tcpAddr, err := net.ResolveTCPAddr(network, addr)
	if err != nil {
		return nil, err
	}

	l, err := net.ListenTCP(network, tcpAddr)
	if err != nil {
		return nil, err
	}
	return &Listener{l}, nil
}

func Memcheck(name string, arg ...string) *exec.Cmd {
	cmd := exec.Command(name, "--tool=memcheck")
	cmd.Args = append(cmd.Args, arg...)
	return cmd
}
