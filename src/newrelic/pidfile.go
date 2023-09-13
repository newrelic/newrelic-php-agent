//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"errors"
	"os"
	"strconv"
	"syscall"

	"newrelic.com/daemon/newrelic/limits"
	"newrelic.com/daemon/newrelic/log"
)

var (
	ErrLocked     = errors.New("pid file lock held by another process")
	ErrRetryLimit = errors.New("max retries exceeded trying to create pid file")
)

// PidFile represents an open file descriptor to a pid file.
//
// A few words about pid files. The daemon requires a pid file to prevent
// a race condition where many agents attempt to spawn a daemon at the
// same time. To fully close the race condition the daemon that holds
// the pid file lock must retain it for its entire lifetime. For that
// reason we do not close the pid file and we do not delete the pid file
// on exit.
type PidFile struct {
	file *os.File
}

// CreatePidFile opens the given pid file and acquires an exclusive
// write lock.
func CreatePidFile(name string) (*PidFile, error) {
	for i := 0; i < limits.MaxPidfileRetries; i++ {
		f, err := os.OpenFile(name, os.O_CREATE|os.O_WRONLY, 0666)
		if err != nil {
			return nil, err
		}

		if err := setWriteLock(f); err != nil {
			f.Close()
			return nil, err
		}

		// There's a race condition between opening the file and acquiring
		// the lock. Consider the following.
		//
		//   1) A opens the pid file and acquires the lock.
		//   2) B opens the pid file.
		//   3) A exits, cleaning up the pid file and releasing the lock.
		//   4) B then acquires a lock on the deleted pid file.
		//   5) C comes along and creates a new pid file...
		//
		// Attempt to detect and handle this and similar situations by retrying.

		pathStat, err := os.Stat(f.Name())
		if err != nil {
			// File is probably gone or inaccessible. Try again.
			log.Debugf("pidfile: %v - retrying", err)
			f.Close()
			continue
		}

		fdStat, err := f.Stat()
		if err != nil {
			// Fatal error (EIO?), give up.
			f.Close()
			return nil, err
		}

		if !os.SameFile(pathStat, fdStat) {
			log.Debugf("pidfile: file has changed since it was opened - retrying")
			f.Close()
			continue
		}

		// Remove stale pid.
		if err := f.Truncate(0); err != nil {
			f.Close()
			return nil, err
		}

		return &PidFile{file: f}, nil
	}

	return nil, ErrRetryLimit
}

// setWriteLock tries to acquire a POSIX exclusive write lock on f.
func setWriteLock(f *os.File) error {
	lock := &syscall.Flock_t{
		Type:   syscall.F_WRLCK,
		Whence: int16(os.SEEK_SET),
	}

	err := syscall.FcntlFlock(f.Fd(), syscall.F_SETLK, lock)
	if err != nil {
		if err == syscall.EACCES || err == syscall.EAGAIN {
			return ErrLocked
		}
		return err
	}

	return nil
}

// Name returns the name of the pid file as presented to CreatePidFile.
func (f *PidFile) Name() string {
	return f.file.Name()
}

// Write writes the process id of the current process to f replacing its
// contents. It returns the number of bytes written and an error, if any.
func (f *PidFile) Write() (n int, err error) {
	if err = f.file.Truncate(0); err != nil {
		return 0, err
	}

	s := strconv.Itoa(os.Getpid()) + "\n"
	return f.file.WriteString(s)
}

// Close closes the pid file, releasing its write lock and rendering it
// unusable for I/O. It returns an error, if any. Note, closing the pid
// file does not cause it to be removed.
func (f *PidFile) Close() error {
	return f.file.Close()
}

// Remove attempts to remove the pid file. Removing a pid file releases its
// write lock and renders the PidFile unusable for I/O. For a daemon process,
// removing the pid file should be the very last step before exiting.
func (f *PidFile) Remove() error {
	if err := os.Remove(f.file.Name()); err != nil {
		return err
	}

	return f.file.Close()
}
