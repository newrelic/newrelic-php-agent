//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package log

import (
	"fmt"
	"io"
	"log"
	"os"
	"runtime"
	"strconv"
	"strings"
	"sync/atomic"
)

type Level int32

const (
	LogAlways Level = iota
	LogError
	LogWarning
	LogInfo
	LogDebug
)

const daemonLogFlags = log.Ldate | log.Ltime | log.Lmicroseconds

var (
	daemonLevel  = LogInfo
	daemonLogPid = "(" + strconv.Itoa(os.Getpid()) + ")"
)

func Init(level Level, location string) error {
	SetLevel(level)

	w, err := openLog(location)
	if err != nil {
		return err
	}

	log.SetFlags(daemonLogFlags)
	log.SetOutput(w)
	return nil
}

func Errorf(format string, a ...interface{}) { logf(LogError, format, a...) }
func Warnf(format string, a ...interface{})  { logf(LogWarning, format, a...) }
func Infof(format string, a ...interface{})  { logf(LogInfo, format, a...) }
func Debugf(format string, a ...interface{}) { logf(LogDebug, format, a...) }

func logf(level Level, format string, a ...interface{}) {
	maxLevel := atomic.LoadInt32((*int32)(&daemonLevel))
	if int32(level) <= maxLevel {
		log.Printf(daemonLogPid+" "+level.String()+": "+format, a...)
	}
}

// SetLevel sets the current log level. It is safe to call this function
// from multiple goroutines.
func SetLevel(level Level) {
	atomic.StoreInt32((*int32)(&daemonLevel), int32(level))
}

var auditLog *log.Logger

func Auditing() bool {
	return nil != auditLog
}

func InitAudit(location string) error {
	w, err := openLog(location)
	if err != nil {
		return err
	}

	auditLog = log.New(w, "", daemonLogFlags)
	return nil
}

func Audit(format string, a ...interface{}) {
	if auditLog != nil {
		auditLog.Printf(format, a...)
	}
}

func openLog(location string) (io.Writer, error) {
	switch location {
	case "stdout":
		return os.Stdout, nil
	case "stderr":
		return os.Stderr, nil
	default:
		return os.OpenFile(location, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0666)
	}
}

// StackTrace formats a stack trace of the calling goroutine.
func StackTrace() []byte {
	buf := make([]byte, 65*1024)
	n := runtime.Stack(buf, false)
	return buf[:n]
}

// String returns the string representation of the Level. It's
// implemented to satisify the Stringer and flag.Value interfaces.
func (level Level) String() string {
	switch level {
	case LogAlways:
		return "Always"
	case LogError:
		return "Error"
	case LogWarning:
		return "Warning"
	case LogInfo:
		return "Info"
	case LogDebug:
		return "Debug"
	default:
		return fmt.Sprintf("Unknown(%d)", level)
	}
}

// Set implements the flag.Value.Set method. This allows the Level
// type to be parsed by the flag package.
func (level *Level) Set(s string) error {
	x, err := parseAxiomLevel(s)
	if err != nil {
		return err
	}

	*level = x
	return nil
}

// UnmarshalText implements the encoding.TextUnmarshaler interface.
// This allows the log level to be unmarshaled by the configuration
// parser.
func (level *Level) UnmarshalText(text []byte) error {
	x, err := parseAxiomLevel(string(text))
	if err != nil {
		return err
	}

	*level = x
	return nil
}

// splitSubsystem parses an Axiom subsystem clause into the subsystem
// name and the log level. If a subsystem name is not present, the
// subsystem is all.
func splitSubsystem(s string) (string, string) {
	halves := strings.SplitN(s, "=", 2)
	if len(halves) == 1 {
		return "all", halves[0]
	}
	return halves[0], halves[1]
}

// parseAxiomLevel parses a log level using the subsystem syntax supported
// by the Axiom library's logging facility. The subsystem syntax is supported
// for backwards compatibility only.
func parseAxiomLevel(s string) (level Level, err error) {
	finalLevel := LogInfo
	splitFunc := func(r rune) bool { return r == ',' || r == ';' }

	for _, clause := range strings.FieldsFunc(s, splitFunc) {
		subsys, levelStr := splitSubsystem(clause)

		level, err = parseLevel(levelStr)
		if err != nil {
			return level, err
		}

		switch strings.ToLower(subsys) {
		case "all", "*":
			// settings for the "all" subsystem replaces the current level
			finalLevel = level
		default:
			// for other subsystems, approximate the Axiom behavior and take
			// the more verbose level.
			if level > finalLevel {
				finalLevel = level
			}
		}
	}

	return finalLevel, nil
}

func parseLevel(s string) (Level, error) {
	switch strings.ToLower(s) {
	case "always":
		return LogAlways, nil
	case "error":
		return LogError, nil
	case "warning":
		return LogWarning, nil
	case "info", "":
		return LogInfo, nil
		// verbose and verbosedebug kept for historical compatibility
	case "debug", "verbose", "verbosedebug":
		return LogDebug, nil
	default:
		return LogInfo, fmt.Errorf("invalid log level: %q", s)
	}
}
