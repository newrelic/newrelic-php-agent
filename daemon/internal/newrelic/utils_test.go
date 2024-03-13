//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"strings"
	"testing"
)

const (
	byteLimit = 255
)

func TestStringLengthByteLimit(t *testing.T) {
	testStrings := map[string]int{
		"awesome":                 7,
		"日本\x80語":                 10, // handles bad unicode
		"":                        0,
		strings.Repeat("!", 1000): 255, // super long host name
		strings.Repeat("!日本", 37): 253, // will not truncate mid-character
	}

	for key, value := range testStrings {
		trunc := stringLengthByteLimit(key, byteLimit)
		if len(trunc) != value {
			t.Errorf("%s: string is not the expected length", trunc)
		}
	}
}
