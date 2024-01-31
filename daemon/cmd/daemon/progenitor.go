//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"os"
	"runtime"
	"syscall"

	// "daemon/signal"
	// "newrelic/log"

	"github.com/newrelic/newrelic-php-agent/daemon/cmd/daemon/signal"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

// runProgenitor starts the watcher and waits for the worker to start listening.
func runProgenitor(cfg *Config) {
	var readyChan <-chan signal.WorkerState

	// Setting the WaitForPort flag to `0` disables the timeout completely.
	if cfg.WaitForPort.Nanoseconds() > 0 {
		readyChan = signal.ListenForWorker(cfg.WaitForPort)
	}

	// Ensure we're in our own process group.
	if err := signal.IsolateProcessGroup(); err != nil {
		log.Warnf("error isolating process group: %v", err)
	}

	if _, err := spawnWatcher(cfg); err != nil {
		// Some older RHEL 5.x linux kernels incorrectly handle missing system
		// calls (here: pipe2), which manifests as an EBADF error when spawning
		// a child process.
		if runtime.GOOS == "linux" {
			perr, ok := err.(*os.PathError)
			if ok && perr.Err == syscall.EBADF {
				err = borkedSyscallError("pipe2")
			}
		}

		log.Errorf("unable to create watcher process: %v", err)
		setExitStatus(1)
		return
	}

	if readyChan != nil {
		switch state := <-readyChan; state {
		case signal.WorkerReady:
			log.Debugf("worker has indicated that it is ready to receive connections; exiting the progenitor")

		case signal.WorkerTimeout:
			log.Errorf("did not receive notice after %v that the worker was ready", cfg.WaitForPort)
			setExitStatus(2)

		default:
			log.Errorf("unexpected worker state value: %v", state)
			setExitStatus(1)
		}
	}
}
