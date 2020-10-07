//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package utilization

import (
	"fmt"
	"strings"
	"time"
)

// Helper constants, functions, and types common to multiple providers are
// contained in this file.

// Constants from the spec.
const (
	maxFieldValueSize = 255             // The maximum value size, in bytes.
	providerTimeout   = 1 * time.Second // The maximum time a HTTP provider
	// may block.
)

// validationError represents a response from a provider endpoint that doesn't
// match the format expectations.
type validationError struct{ e error }

func (a validationError) Error() string {
	return a.e.Error()
}

func isValidationError(e error) bool {
	_, is := e.(validationError)
	return is
}

// This function normalizes string values.
func normalizeValue(s string) (string, error) {
	out := strings.TrimSpace(s)

	// Note: We define the length in bytes not codepoints.
	bytes := []byte(out)
	if len(bytes) > maxFieldValueSize {
		return "", validationError{fmt.Errorf(
			"response is too long: got %d; expected <=%d",
			len(bytes), maxFieldValueSize)}
	}

	for i, r := range out {
		if !isAcceptableRune(r) {
			return "", validationError{fmt.Errorf(
				"bad character %x at position %d in response", r, i)}
		}
	}

	return out, nil
}

func isAcceptableRune(r rune) bool {
	switch r {
	case 0xFFFD:
		return false // invalid UTF-8
	case '_', ' ', '/', '.', '-':
		return true
	default:
		return r > 0x7f ||
			('0' <= r && r <= '9') ||
			('a' <= r && r <= 'z') ||
			('A' <= r && r <= 'Z')
	}
}
