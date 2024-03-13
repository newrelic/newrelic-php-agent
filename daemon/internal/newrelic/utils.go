//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

// stringLengthRuneLimit truncates strings using a character-limit boundary and
// avoids terminating in the middle of a multibyte character.
func stringLengthRuneLimit(str string, runeLimit int) string {
	runes := []rune(str)
	if len(runes) < runeLimit {
		return str
	}
	return string(runes[0:runeLimit])
}

// stringLengthByteLimit truncates strings using a byte-limit boundary and
// avoids terminating in the middle of a multibyte character.
func stringLengthByteLimit(str string, byteLimit int) string {
	if len(str) <= byteLimit {
		return str
	}

	limitIndex := 0
	for pos := range str {
		if pos > byteLimit {
			break
		}
		limitIndex = pos
	}
	return str[0:limitIndex]
}
