//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package config

import (
	"time"
	"unicode/utf8"
)

// A Timeout specifies a time limit as a non-negative int64 nanosecond count.
// The representation limits the largest representable time limit to
// approximately 290 years.
type Timeout int64

// UnmarshalText implements the encoding.TextUnmarshaler interface. The timeout
// is expected to be non-negative. Terms without a unit are interpreted as
// seconds.
func (t *Timeout) UnmarshalText(text []byte) error {
	var x time.Duration
	var err error

	if r, _ := utf8.DecodeLastRune(text); r >= '0' && r <= '9' {
		// Input does not end with a unit. Assume milliseconds instead of
		// nanoseconds to match the behavior of axiom.
		x, err = time.ParseDuration(string(text) + "ms")
	} else {
		x, err = time.ParseDuration(string(text))
	}

	if err != nil {
		return err
	}

	*t = Timeout(x)
	return nil
}

// String returns a string representing the timeout in the form "72h3m0.5s".
// Leading zero units are omitted. As a special case, durations less than one
// second format use a smaller unit (milli-, micro-, or nanoseconds) to ensure
// that the leading digit is non-zero. The zero duration formats as 0s.
func (t Timeout) String() string {
	if t > 0 {
		return time.Duration(t).String()
	}
	return "0s"
}
