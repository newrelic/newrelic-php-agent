//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"testing"
)

func TestValidateBootID(t *testing.T) {
	testCases := []struct {
		name     string
		input    []byte
		expected string
		isError  bool
	}{
		{
			name:     "empty string",
			input:    []byte(""),
			expected: "",
			isError:  false,
		},
		{
			name:     "36 byte string",
			input:    []byte("bea754c9-b08f-479f-878b-bc66005a36bd"),
			expected: "bea754c9-b08f-479f-878b-bc66005a36bd",
			isError:  false,
		},
		{
			name:     "36 byte string with newlines",
			input:    []byte("bea754c9-b08f-479f-878b-bc66005a36bd\n"),
			expected: "bea754c9-b08f-479f-878b-bc66005a36bd",
			isError:  false,
		},
		{
			name:     "128 byte string",
			input:    []byte("01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567"),
			expected: "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567",
			isError:  false,
		},
		{
			name:     "129 byte string",
			input:    []byte("012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"),
			expected: "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567",
			isError:  false,
		},
		{
			name:     "string with control characters",
			input:    []byte{0x0, 0x1, 0x2},
			expected: "",
			isError:  true,
		},
		{
			name:     "string with high-bit characters",
			input:    []byte{0x80},
			expected: "",
			isError:  true,
		},
	}

	for _, tc := range testCases {
		actual, err := validateBootID(tc.input)
		if tc.isError && err == nil {
			t.Fatalf("%s: expected error; got nil", tc.name)
		} else if !tc.isError && err != nil {
			t.Fatalf("%s: expected no error; got: %v", tc.name, err)
		} else if !tc.isError && actual != tc.expected {
			t.Fatalf("%s: expected %s; got %s", tc.name, tc.expected, actual)
		}
	}
}
