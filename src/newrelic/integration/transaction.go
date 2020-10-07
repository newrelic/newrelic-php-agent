//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"net/http/cgi"
	"net/http/httptest"
	"net/url"
	"newrelic/log"
	"os"
	"os/exec"
	"path/filepath"
)

// A Tx represents a transaction.
type Tx interface {
	Execute() (headers http.Header, body []byte, err error)
}

// A Script is a file or code fragment executed during a transaction.
type Script interface {
	Dir() string   // working directory
	Name() string  // script file name
	Bytes() []byte // script content for code fragments
	IsFile() bool  // true for real files
}

func flatten(x map[string]string) []string {
	s := make([]string, 0, len(x))
	for k, v := range x {
		s = append(s, k+"="+v)
	}
	return s
}

// PhpTx constructs non-Web transactions to be executed by PHP.
func PhpTx(src Script, env, settings map[string]string, ctx *Context) (Tx, error) {
	// Note: file path must be relative to the working directory.
	var txn Tx

	args := phpArgs(nil, filepath.Base(src.Name()), false, settings)

	if ctx.Valgrind != "" {
		txn = &ValgrindCLI{
			CLI: CLI{
				Path: ctx.PHP,
				Dir:  src.Dir(),
				Env:  flatten(env),
				Args: args,
			},
			Valgrind: ctx.Valgrind,
			Timeout:  ctx.Timeout,
		}
	} else {
		txn = &CLI{
			Path: ctx.PHP,
			Dir:  src.Dir(),
			Env:  flatten(env),
			Args: args,
		}
	}

	if !src.IsFile() {
		return withTempFile(txn, src.Name(), src.Bytes()), nil
	}
	return txn, nil
}

// CgiTx constructs Web transactions to be executed as an HTTP request
// using the CGI/1.1 protocol. Like CLI transactions, the lifetime
// of a CGI transaction is the same as the process that executes it.
//
// See: https://tools.ietf.org/html/rfc3875
func CgiTx(src Script, env, settings map[string]string, headers http.Header, ctx *Context) (Tx, error) {
	var err error

	req := &http.Request{
		Method:     env["REQUEST_METHOD"],
		RequestURI: "/" + filepath.Base(src.Name()) + "?" + env["QUERY_STRING"],
		Proto:      "HTTP/1.1",
		ProtoMajor: 1,
		ProtoMinor: 1,
		Header:     headers,
	}

	req.URL, err = url.ParseRequestURI(req.RequestURI)
	if err != nil {
		return nil, fmt.Errorf("unable to create cgi request: %v", err)
	}

	req.URL.Scheme = "http"
	req.URL.Host = "127.0.0.1"
	req.Host = "127.0.0.1"

	scriptFile, err := filepath.Abs(src.Name())
	if err != nil {
		return nil, fmt.Errorf("unable to create cgi request: %v", err)
	}

	tx := &CGI{
		request: req,
		handler: &cgi.Handler{
			Path: ctx.CGI,
			Dir:  src.Dir(),
			Args: phpArgs(nil, "", false, settings),
		},
	}

	tx.handler.Env = append(tx.handler.Env,
		"SCRIPT_FILENAME="+scriptFile,
		"SCRIPT_NAME=/"+filepath.Base(src.Name()),
		"REDIRECT_STATUS=200")
	tx.handler.Env = append(tx.handler.Env, flatten(env)...)

	// If the environment includes a REQUEST_URI and doesn't include an explicit
	// PATH_INFO, we'll set it here to avoid problems with Go's CGI package
	// attempting to guess what the PATH_INFO should be.
	if ruri, ok := env["REQUEST_URI"]; ok {
		if _, ok := env["PATH_INFO"]; !ok {
			tx.handler.Env = append(tx.handler.Env, "PATH_INFO="+ruri)
		}
	}

	if !src.IsFile() {
		return withTempFile(tx, src.Name(), src.Bytes()), nil
	}
	return tx, nil
}

// phpArgs builds the command line for a PHP process.
func phpArgs(args []string, file string, noConfig bool, settings map[string]string) []string {
	if noConfig {
		args = append(args, "-n")
	}

	for k, v := range settings {
		args = append(args, "-d", k+"="+v)
	}

	if len(file) > 0 {
		args = append(args, "-f", file)
	}

	return args
}

// skipper is a stub transaction that always results in a skip message.
type skipper struct {
	msg string
}

func (tx skipper) Execute() (http.Header, []byte, error) {
	if len(tx.msg) > 0 {
		return nil, []byte(tx.msg), nil
	}
	return nil, []byte("skip: transaction type not supported"), nil
}

// CLI represents a transaction whose lifetime is the same as the
// process that executes it. For example, a PHP CLI invocation.
type CLI struct {
	Path  string   // path to the executable
	Dir   string   // working directory for the process
	Env   []string // specifies the environment of the process
	Args  []string // arguments to pass to the process
	Stdin []byte   // data to pass to the process via stdin
}

func (tx *CLI) Execute() (http.Header, []byte, error) {
	if len(tx.Path) == 0 {
		return nil, []byte("skip: executable not specified"), nil
	}

	cmd := exec.Command(tx.Path)
	if len(tx.Args) > 0 {
		cmd.Args = append(cmd.Args, tx.Args...)
	}
	cmd.Dir = tx.Dir
	cmd.Env = tx.Env

	if len(tx.Stdin) > 0 {
		cmd.Stdin = bytes.NewReader(tx.Stdin)
	}

	log.Debugf("command: %v", cmd)

	output, err := cmd.CombinedOutput()
	return nil, output, err
}

// CGI represents a Web transaction to be executed as an HTTP request
// using the CGI/1.1 protocol. Like CLI transactions, the lifetime
// of a CGI transaction is the same as the process that executes it.
//
// See: https://tools.ietf.org/html/rfc3875
type CGI struct {
	handler *cgi.Handler
	request *http.Request
}

func (tx *CGI) Execute() (http.Header, []byte, error) {
	resp := httptest.NewRecorder()
	tx.handler.ServeHTTP(resp, tx.request)
	return resp.HeaderMap, resp.Body.Bytes(), nil
}

// withTempFile wraps tx and creates a temporary file called name
// containing data that exists for the duration of the transaction.
func withTempFile(tx Tx, name string, data []byte) Tx {
	return &tempFileWrapper{tx, name, data}
}

// tempFileWrapper wraps transactions that depend on a temp file. The
// temp file is created before executing the wrapped transaction, and
// subsequently deleted if the transaction succeeds. On error, the
// file is left in place to aid debugging.
type tempFileWrapper struct {
	tx   Tx     // wrapped transaction
	name string // temp file name
	data []byte // temp file content
}

func (tx *tempFileWrapper) Execute() (http.Header, []byte, error) {
	if err := tx.writeTempFile(); err != nil {
		return nil, nil, err
	}

	defer os.Remove(tx.name)

	return tx.tx.Execute()
}

func (tx *tempFileWrapper) writeTempFile() error {
	f, err := os.Create(tx.name)
	if err != nil {
		return err
	}
	defer f.Close()

	_, err = io.Copy(f, bytes.NewReader(tx.data))
	if err != nil {
		os.Remove(f.Name())
	}
	return err
}

// A ScriptFile is the path to a file to be executed as a transaction.
// A typical use for a scriptFile is passing the path to a script to
// execute to the interpreter via command line argument.
type ScriptFile string

func (s ScriptFile) Dir() string   { return filepath.Dir(string(s)) }
func (s ScriptFile) Name() string  { return string(s) }
func (s ScriptFile) Bytes() []byte { return nil }
func (s ScriptFile) IsFile() bool  { return true }

// A ScriptFragment is a script fragment to be executed as a transaction.
// A typical use for a scriptFragment is executing a SKIPIF directive by
// writing the fragment to a temporary file for execution by the
// interpreter.
type ScriptFragment struct {
	name string
	data []byte
}

func (s *ScriptFragment) Dir() string   { return filepath.Dir(s.name) }
func (s *ScriptFragment) Name() string  { return s.name }
func (s *ScriptFragment) Bytes() []byte { return s.data }
func (s *ScriptFragment) IsFile() bool  { return false }
