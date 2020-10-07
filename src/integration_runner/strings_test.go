//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"testing"
)

func TestReplaceLast(t *testing.T) {
	tests := []struct {
		in          string
		old         string
		replacement string
		n           int
		expected    string
	}{
		// Happy path.
		{
			"foo",
			"o",
			"b",
			1,
			"fob",
		},
		// Removal.
		{
			"foo",
			"o",
			"",
			-1,
			"f",
		},
		// No substitutions.
		{
			"foo",
			"o",
			"b",
			0,
			"foo",
		},
		// Substring doesn't exist.
		{
			"foo",
			"x",
			"b",
			1,
			"foo",
		},
		// Empty input.
		{
			"",
			"o",
			"b",
			1,
			"",
		},
		// Empty search string.
		{
			"foo",
			"",
			"x",
			1,
			"foox",
		},
	}

	for _, test := range tests {
		actual := stringReplaceLast(test.in, test.old, test.replacement, test.n)
		if test.expected != actual {
			t.Errorf("Replacement of '%s' to '%s' in '%s' (%d time(s)) failed: expected '%s', got '%s'",
				test.old, test.replacement, test.in, test.n, test.expected, actual)
		}
	}
}

func TestReverse(t *testing.T) {
	testStrings := map[string]string{
		"foo": "oof",
		"你好":  "好你",
		"a":   "a",
		"":    "",
	}

	for in, expected := range testStrings {
		actual := stringReverse(in)
		if expected != actual {
			t.Errorf("Reverse of '%s' failed: expected '%s', got '%s'", in, expected, actual)
		}
	}
}
