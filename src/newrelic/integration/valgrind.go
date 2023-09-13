//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bytes"
	"fmt"
	"io/ioutil"
	"net/http"
	"sync"
	"time"

	"newrelic.com/daemon/newrelic/integration/valgrind"
	"newrelic.com/daemon/newrelic/log"
)

type ValgrindCLI struct {
	CLI
	Valgrind string
	Timeout  time.Duration
}

func (tx *ValgrindCLI) Execute() (http.Header, []byte, error) {
	if len(tx.Path) == 0 {
		return nil, []byte("skip: executable not specified"), nil
	}

	// For now, we don't have a mechanism to handle concurrent invocations
	// of Valgrind. In the future, we could use the appname to connect
	// valgrind reports to their tests in the same way we use the appname
	// to link the transaction data. Failing that, perhaps we could use
	// the valgrind process's pid instead.
	valgrindMu.Lock()
	defer valgrindMu.Unlock()

	cmd := valgrind.Memcheck(tx.Valgrind, "--quiet")
	cmd.Args = append(cmd.Args, "--xml=yes")
	cmd.Args = append(cmd.Args, "--xml-socket="+valgrindLn.Addr().String())
	cmd.Args = append(cmd.Args, "--")
	cmd.Args = append(cmd.Args, tx.Path)
	if len(tx.Args) > 0 {
		cmd.Args = append(cmd.Args, tx.Args...)
	}
	cmd.Dir = tx.Dir
	cmd.Env = tx.Env

	log.Debugf("command: %v", cmd)

	if len(tx.Stdin) > 0 {
		cmd.Stdin = bytes.NewReader(tx.Stdin)
	}

	ch := make(chan resultOrError, 1)
	go func() {
		var result resultOrError
		result.R, result.E = acceptOneReport(tx.Timeout)
		ch <- result
	}()

	output, err := cmd.CombinedOutput()
	log.Debugf("command executed in %s", cmd.ProcessState.SystemTime()+cmd.ProcessState.UserTime())

	vgOutput := <-ch

	// Append the output from Valgrind to the test output.
	//
	// TODO: Eventually we want to report valgrind output separately, so we can
	// treat valgrind errors similarly to failed test expectations.  i.e. We'd
	// like to add them to Test.Failures. After all, each test has an implicit
	// expectation that it will not exhibit memory bugs!
	//
	// TODO: Once the former is in place, we should be able to parse the memory
	// leak reports produced by the Zend Memory Manager from the test output and
	// report them the same way as valgrind errors.
	if vgOutput.R != nil && len(vgOutput.R.Errors) > 0 {
		// Safe to ignore the error here, Report.MarshalText() never fails.
		data, _ := vgOutput.R.MarshalText()
		output = append(output, '\n')
		output = append(output, data...)
	}

	// If both the test and valgrind resulted in an error, choose the test
	// error to report.
	//
	// TODO: Marry the two errors in a small, private type so the developers can
	// live happily ever after.
	if err == nil && vgOutput.E != nil {
		err = vgOutput.E
	}

	// Ensure a non-nil error is returned when valgrind detects errors.
	// Otherwise, the test could be marked as passing if it does not have
	// any expectations on the test output. This sucks.
	//
	// TODO: Remove this when valgrind errors can be treated as failed test
	// expectations.
	if err == nil && vgOutput.R != nil && len(vgOutput.R.Errors) > 0 {
		err = fmt.Errorf("detected %d memory errors", len(vgOutput.R.Errors))
	}

	return nil, output, err
}

// resultOrError is a poor man's sum type.
type resultOrError struct {
	R *valgrind.Report
	E error
}

var (
	valgrindMu sync.Mutex
	valgrindLn *valgrind.Listener
)

// acceptOneReport accepts a single connection from a valgrind process,
// reads its commentary (which is expected to be XML formatted), and
// returns it as a Report.
func acceptOneReport(timeout time.Duration) (*valgrind.Report, error) {
	var deadline time.Time

	if timeout > 0 {
		deadline = time.Now().Add(timeout)
	}

	if !deadline.IsZero() {
		valgrindLn.SetDeadline(deadline)
		defer valgrindLn.SetDeadline(time.Time{})
	}

	conn, err := valgrindLn.Accept()
	if err != nil {
		return nil, err
	}

	defer conn.Close()
	conn.SetReadDeadline(deadline)

	output, err := ioutil.ReadAll(conn)
	if err != nil {
		return nil, err
	}

	return valgrind.ParseXML(output)
}

func init() {
	l, err := valgrind.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	valgrindLn = l
}
