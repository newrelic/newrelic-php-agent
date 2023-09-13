//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"runtime"
	"syscall"

	"newrelic.com/daemon/newrelic/log"
)

type workerState struct {
	status syscall.WaitStatus
	err    error
}

// runWatcher spawns and supervises worker processes. When a worker exits
// unexpectedly, it is respawned. Only a single worker process should
// exist at any given time.
func runWatcher(cfg *Config) {
	signalChan := make(chan os.Signal, 1)
	signal.Notify(signalChan, syscall.SIGTERM)

	// until we're told to shutdown or the worker exits cleanly
	//   spawn a worker
	//   wait for worker to exit or a signal to arrive
	//     worker exit status 0, exit with success
	//     worker exit status 1, exit with ???
	//     worker exit status >= 2, respawn worker
	//     SIGTERM: stop worker and exit with success

	for {
		worker, err := spawnWorker()
		if err != nil {
			// Some older RHEL 5.x linux/CentOS 5.x Xen  kernels incorrectly handle
			// missing system calls (here: pipe2), which manifests as an EBADF
			// error when spawning a child process.
			if runtime.GOOS == "linux" {
				perr, ok := err.(*os.PathError)
				if ok && perr.Err == syscall.EBADF {
					err = borkedSyscallError("pipe2")
				}
			}

			log.Errorf("unable to create worker: %v", err)
			setExitStatus(1)
			return
		}

		select {
		case status := <-supervise(worker):
			if status != nil && status.ShouldRespawn() {
				log.Errorf("%v - restarting", status)
			} else {
				log.Infof("%v - NOT restarting", status)
				return
			}
		case caught := <-signalChan:
			log.Infof("watcher received signal %d - exiting", caught)
			worker.Process.Signal(caught)
			return
		}
	}
}

// spawnWorker starts a new worker process.
func spawnWorker() (*exec.Cmd, error) {
	env := Environment(os.Environ())
	env.Set(RoleEnvironmentVariable, "worker")

	cmd := exec.Command(os.Args[0], os.Args[1:]...)
	cmd.Env = []string(env)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	// Only one process can own the pid file, and if this function
	// is being called that should be the current process.
	cmd.Args = append(cmd.Args, "-no-pidfile")

	return cmd, cmd.Start()
}

// supervise monitors a single worker process.
func supervise(worker *exec.Cmd) chan *workerState {
	if worker == nil {
		panic("worker is nil")
	}

	statusChan := make(chan *workerState)

	go func() {
		if err := worker.Wait(); err != nil {
			if _, ok := err.(*exec.ExitError); ok {
				statusChan <- newWorkerState(worker.ProcessState)
			} else {
				statusChan <- &workerState{err: err}
			}
		} else {
			statusChan <- newWorkerState(worker.ProcessState)
		}

		close(statusChan)
	}()

	return statusChan
}

// ShouldRespawn determines whether a worker should be respawned based on
// how it terminated.
func (s *workerState) ShouldRespawn() bool {
	if s.err != nil {
		return true
	}

	switch st := s.status; {
	case st.Exited():
		return st.ExitStatus() >= 2
	case st.Signaled():
		return st.Signal() != syscall.SIGTERM
	default:
		return true
	}
}

func (s *workerState) String() string {
	if s.err != nil {
		return s.err.Error()
	}

	switch st := s.status; {
	case st.Exited():
		return fmt.Sprintf("worker exited with status %d", st.ExitStatus())
	case st.Signaled():
		if st.CoreDump() {
			return fmt.Sprintf("worker received signal %s (core dumped)", st.Signal())
		}
		return fmt.Sprintf("worker received signal %s", st.Signal())
	default:
		return "worker exited with unknown status"
	}
}

func newWorkerState(ps *os.ProcessState) *workerState {
	if ws, ok := ps.Sys().(syscall.WaitStatus); ok {
		return &workerState{status: ws}
	}

	if ps.Success() {
		return &workerState{}
	}

	return &workerState{err: errors.New(ps.String())}
}
