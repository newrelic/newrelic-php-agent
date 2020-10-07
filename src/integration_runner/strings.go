//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"strings"
	"unicode/utf8"
)

// stringReplaceLast replaces the last occurrence(s) of a substring with
// something else. Other than the order in which the string is searched, the
// semantics of this function are identical to strings.Replace().
func stringReplaceLast(s string, old string, replacement string, n int) string {
	return stringReverse(strings.Replace(stringReverse(s), stringReverse(old), stringReverse(replacement), n))
}

// stringReverse reverses a string. Note that combining characters will
// probably be handled badly.
func stringReverse(s string) string {
	n := utf8.RuneCountInString(s)
	runes := make([]rune, n)
	for _, c := range s {
		n--
		runes[n] = c
	}
	return string(runes)
}
