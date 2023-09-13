//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"context"
	"errors"
	"expvar"
	"fmt"
	"net"
	"net/http"
	_ "net/http/pprof" // enable profiling api
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"time"

	ds "newrelic.com/daemon/daemon/signal"

	"newrelic.com/daemon/newrelic"
	"newrelic.com/daemon/newrelic/config"
	"newrelic.com/daemon/newrelic/limits"
	"newrelic.com/daemon/newrelic/log"
	"newrelic.com/daemon/newrelic/version"
)

type workerError struct {
	Component string // component that failed, e.g. listener
	Respawn   bool   // restart process?
	Err       error  // underlying cause
}

func (e *workerError) Error() string {
	if e == nil {
		return "<nil>"
	}
	return e.Component + ": " + e.Err.Error()
}

// runWorker starts the listener and processor and does not return until a
// signal is received indicating the worker should shut down, or a fatal
// error occurs.
func runWorker(cfg *Config) {
	if cfg.MaxFiles > 0 {
		raiseFileLimit(cfg.MaxFiles)
	}

	if cfg.AuditFile != "" {
		if err := log.InitAudit(cfg.AuditFile); err != nil {
			log.Errorf("unable to open audit log: %v", err)
			setExitStatus(1)
			return
		}
	}

	if 0 != cfg.PProfPort {
		addr := net.JoinHostPort("127.0.0.1", strconv.Itoa(cfg.PProfPort))

		log.Infof("pprof enabled at %s", addr)

		// Uncomment this to enable the /debug/pprof/block endpoint.
		// This adds measurable overhead to the daemon, so only enable
		// it when necessary.
		// runtime.SetBlockProfileRate(1)

		// Publish version info.
		versionInfo := &struct{ Number, Build string }{version.Number, version.Commit}
		expvar.Publish("version", expvar.Func(func() interface{} { return versionInfo }))

		go func() {
			err := http.ListenAndServe(addr, nil)
			if err != nil {
				log.Debugf("pprof server error: %v", err)
			}
		}()
	}

	errorChan := make(chan error)
	signalChan := make(chan os.Signal, 1)
	signal.Notify(signalChan, syscall.SIGTERM)
	if cfg.Foreground {
		signal.Notify(signalChan, syscall.SIGINT)
	}

	clientCfg := &newrelic.ClientConfig{
		CAFile: cfg.CAFile,
		CAPath: cfg.CAPath,
		Proxy:  cfg.Proxy,
	}

	log.Infof("collector configuration is %+v", clientCfg)

	client, err := newrelic.NewClient(clientCfg)
	if nil != err {
		log.Errorf("unable to create client: %v", err)
		setExitStatus(1)
		return
	}

	if cfg.AppTimeout < 0 {
		cfg.AppTimeout = config.Timeout(limits.DefaultAppTimeout)
		log.Errorf("application inactivity timeout cannot be negative, using default of %v",
			cfg.AppTimeout)
	}

	p := newrelic.NewProcessor(newrelic.ProcessorConfig{
		Client:          client,
		IntegrationMode: cfg.IntegrationMode,
		UtilConfig:      cfg.MakeUtilConfig(),
		AppTimeout:      time.Duration(cfg.AppTimeout),
	})
	go processTxnData(errorChan, p)

	// We have a progenitor if neither the --foreground nor
	// --watchdog-foreground flags were supplied.
	hasProgenitor := !(cfg.Foreground || cfg.WatchdogForeground)

	ctx, cancel := context.WithCancel(context.Background())

	select {
	case <-listenAndServe(ctx, cfg.BindAddr, errorChan, p, hasProgenitor):
		log.Debugf("listener shutdown - exiting")
	case err := <-errorChan:
		if err != nil {
			log.Errorf("%v", err)
			if we, ok := err.(*workerError); ok && we.Respawn {
				setExitStatus(3)
			} else {
				setExitStatus(1)
			}
		}
	case caught := <-signalChan:
		// Close the listener before sending remaining data. This ensures that the socket
		// connection is closed as soon as possible and other processes can start listening
		// the socket while remaining data is sent.
		cancel()
		log.Infof("worker received signal %d - sending remaining data", caught)
		p.CleanExit()
		log.Infof("worker sent remaining data, now exiting")
	}
}

// listenAndServe starts and supervises the listener. If the listener
// terminates with an error, it is sent on errorChan; otherwise, the
// returned channel is closed to indicate a clean exit.
func listenAndServe(ctx context.Context, address string, errorChan chan<- error, p *newrelic.Processor, hasProgenitor bool) <-chan struct{} {
	doneChan := make(chan struct{})

	go func() {
		defer crashGuard("listener", errorChan)

		addr, err := parseBindAddr(address)
		if err != nil {
			errorChan <- &workerError{Component: "listener", Err: err}
			return
		}

		if addr.Network() == "unix" && !strings.HasPrefix(addr.String(), "@") {
			err := os.Remove(addr.String())
			if err != nil && !os.IsNotExist(err) {
				errorChan <- &workerError{
					Component: "listener",
					Err: fmt.Errorf("unable to remove stale sock file: %v"+
						" - another daemon may already be running?", err),
				}
				return
			}
		}

		list, err := newrelic.Listen(addr.Network(), addr.String())
		if err != nil {
			respawn := true

			// Some older RHEL 5.x linux kernels incorrectly handle missing system
			// calls (here: epoll_create1), which manifests as an EBADF error when
			// creating the listener socket.
			if runtime.GOOS == "linux" {
				perr, ok := err.(*net.OpError)
				if ok && perr.Err == syscall.EBADF {
					respawn = false
					err = borkedSyscallError("epoll_create1")
				}
			}

			// Let's not retry for non-temporary errors (e. g. if the address is
			// already in use).
			perr, ok := err.(*net.OpError)
			if ok && !perr.Temporary() {
				respawn = false
				log.Errorf("received error, no respawning: %v", perr)
			}

			errorChan <- &workerError{
				Component: "listener",
				Respawn:   respawn,
				Err:       err,
			}

			return
		}

		go func() {
			select {
			case <-ctx.Done():
				list.Close()
			}
		}()

		defer list.Close()
		log.Infof("daemon listening on %s", addr)

		if hasProgenitor {
			if err := ds.SendReady(); err != nil {
				log.Debugf("error sending signal to the progenitor process that the worker is ready: %v", err)
			}
		}

		if err = list.Serve(newrelic.CommandsHandler{Processor: p}); err != nil {
			errorChan <- &workerError{
				Component: "server",
				Respawn:   true,
				Err:       err,
			}

			return
		}

		close(doneChan)
	}()

	return doneChan
}

// processTxnData starts and supervises the processor. We expect the
// processor to run for the lifetime of the process. Therefore, if the
// processor terminates, it is treated as a fatal error.
func processTxnData(errorChan chan<- error, p *newrelic.Processor) {
	defer crashGuard("processor", errorChan)

	err := p.Run()
	if err != nil {
		errorChan <- &workerError{
			Component: "processor",
			Respawn:   true,
			Err:       err,
		}
	}
}

func crashGuard(component string, errorChan chan<- error) {
	if err := recover(); err != nil {
		// Stacktraces captured during a panic are handled differently:  This
		// will contain the stack frame where the panic originated.
		stack := log.StackTrace()
		errorChan <- &workerError{
			Component: component,
			Respawn:   true,
			Err:       fmt.Errorf("panic %v\n%s", err, stack),
		}
	}
}

// parseBindAddr parses and validates the listener address.
func parseBindAddr(s string) (address net.Addr, err error) {
	const maxUnixLen = 106

	// '@' prefix specifies a Linux abstract domain socket.
	if runtime.GOOS == "linux" && strings.HasPrefix(s, "@") {
		if len(s) > maxUnixLen {
			return nil, fmt.Errorf("sock file length must be less than %d characters", maxUnixLen)
		}
		return &net.UnixAddr{Name: s, Net: "unix"}, nil
	}

	if strings.Contains(s, "/") {
		if !filepath.IsAbs(s) {
			return nil, errors.New("sock file must be an absolute path")
		} else if len(s) > maxUnixLen {
			return nil, fmt.Errorf("sock file length must be less than %d characters", maxUnixLen)
		}
		return &net.UnixAddr{Name: s, Net: "unix"}, nil
	}

	// For TCP, the supplied address string, s, is one of a port, a :port, or a host:port.
	ip, port := net.IPv4(127, 0, 0, 1), 0

	if strings.Contains(s, ":") {
		host, portString, err := net.SplitHostPort(s)
		if err != nil {
			return nil, fmt.Errorf("invalid addr %q - must be provided as host:port", s)
		}
		if host != "" {
			ip = net.ParseIP(host)
		}

		port, err = strconv.Atoi(portString)
	} else {
		port, err = strconv.Atoi(s)
	}

	if err != nil || port < 1 || port > 65534 {
		return nil, fmt.Errorf("invalid port %d - must be between 1 and 65534", port)
	}
	return &net.TCPAddr{IP: ip, Port: port}, nil
}

// raiseFileLimit attempts to raise the soft limit for open file
// descriptors to be at least n. If the proposed minimum is larger than
// the hard limit an attempt will also be made to raise the hard limit.
// Raising the hard limit requires super-user privileges.
func raiseFileLimit(n uint64) {
	softLimit, hardLimit, err := getFileLimits()
	if err != nil {
		log.Warnf(`unable to increase file limit, the current value could`+
			` not be retrieved. If you are using an init script to start`+
			` the New Relic Daemon trying adding "ulimit -n %d" to your`+
			` init script. The error was %v.`, n, err)
		return
	}

	if n <= softLimit {
		return
	}

	// Maintain the following invariant: softLimit <= hardLimit
	// Failure to abide makes The Dude sad. Also causes EINVAL.

	if n > hardLimit {
		// The hard limit also needs to be raised. Try to raise the soft and
		// hard limits at the same time.
		err := setFileLimits(n, n)
		if err == nil {
			log.Infof("increased file limit to %d", n)
			return
		}

		// Couldn't raise the hard limit. Log the failure and fall through
		// below to raise the soft limit as high as we can.
		if err == syscall.EPERM {
			log.Warnf("unable to increase file hard limit from %d to %d."+
				" Raising the hard limit requires super-user privileges,"+
				" please contact your system administrator for assistance."+
				" An attempt will be made to raise the soft limit to %[1]d.",
				hardLimit, n)
		} else {
			log.Warnf("unable to increase file hard limit from %d to %d."+
				" The error was %v. An attempt will be made to raise the"+
				" soft limit to %[1]d.", hardLimit, n, err)
		}
	}

	softLimit = n

	// Ensure softLimit = min(n, hardLimit). We may have failed to raise
	// the hard limit to be greater than or equal to n above.
	if softLimit > hardLimit {
		softLimit = hardLimit
	}

	err = setFileLimits(softLimit, hardLimit)
	if err != nil {
		log.Warnf("unable to increase file limit: %v", err)
		return
	}

	log.Infof("increased file limit to %d", softLimit)
}
