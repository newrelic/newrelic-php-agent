//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// Package signal provides helpers to allow worker processes to signal to their
// progenitors that they are ready to receive connections.
package signal

import (
	"os"
	"os/signal"
	"syscall"
	"time"
)

// The worker state: one of WorkerReady or WorkerTimeout.
type WorkerState int

const (
	// The state that indicates that the worker is ready to receive connections.
	WorkerReady WorkerState = iota

	// The state indicating that no signal was received before the timeout.
	WorkerTimeout
)

// The signal used to indicate that the worker is ready.
//
// Note that this signal will be sent to all processes in the process group, so
// it needs to be something that is otherwise ignored.
//
// This will probably need to be something else on non-POSIX platforms.
const workerReadySignal = syscall.SIGUSR1

// IsolateProcessGroup ensures that the current process is a process group
// leader.
func IsolateProcessGroup() error {
	return syscall.Setpgid(0, 0)
}

// ListenForWorker waits for a worker process to send it a signal, then uses
// that to indicate that the worker is ready. If no signal is received before
// the timeout duration is hit, then a timeout will be indicated via the
// WorkerState channel.
//
// Regardless, exactly one message will be sent down the returned channel,
// after which it will be closed.
func ListenForWorker(timeout time.Duration) <-chan WorkerState {
	// Set up the signal handler.
	signalChan := make(chan os.Signal, 1)
	signal.Notify(signalChan, workerReadySignal)

	// Set up the timeout timer.
	timer := time.NewTimer(timeout)

	// Finally, set up the channel that we'll use to return the actual state.
	readyChan := make(chan WorkerState)

	// Spawn a goroutine to receive either the signal or timeout: whatever
	// happens first wins.
	go func() {
		select {
		case <-signalChan:
			readyChan <- WorkerReady

		case <-timer.C:
			readyChan <- WorkerTimeout
		}

		close(readyChan)
		close(signalChan)
	}()

	return readyChan
}

// SendReady sends a signal to the progenitor process that the worker is ready.
func SendReady() error {
	return syscall.Kill(0, workerReadySignal)
}
