//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"testing"
)

func isFuzzyMatchTestcase(t *testing.T, s1, s2 string, isMatch bool) {
	err := IsFuzzyMatchString(s1, s2)

	if isMatch {
		if nil != err {
			t.Fatalf("matching %v to %v: unexpected matching error: %v", s1, s2, err)
		} else {
			return
		}
	}

	if nil == err {
		t.Fatalf("matching %v to %v: missing error", s1, s2)
	}
}

func TestIsFuzzyMatch(t *testing.T) {
	isFuzzyMatchTestcase(t, `1`, `2`, false)
	isFuzzyMatchTestcase(t, `"hello"`, `"hi there"`, false)
	isFuzzyMatchTestcase(t, `true`, `false`, false)
	isFuzzyMatchTestcase(t, `1`, `1`, true)
	isFuzzyMatchTestcase(t, `true`, `true`, true)
	isFuzzyMatchTestcase(t, `[]`, `true`, false)
	isFuzzyMatchTestcase(t, `[1]`, `[1,2]`, false)
	isFuzzyMatchTestcase(t, `[1]`, `[true]`, false)
	isFuzzyMatchTestcase(t, `{}`, `{}`, true)
	isFuzzyMatchTestcase(t, `{"zip":1}`, `{}`, false)
	isFuzzyMatchTestcase(t, `{"zip":1}`, `{"zip":1,"zap":2}`, false)
	isFuzzyMatchTestcase(t, `{"zip":true}`, `{"zip":false}`, false)
	isFuzzyMatchTestcase(t, `{"zip":true}`, `{"zap":true}`, false)
	isFuzzyMatchTestcase(t, `{"zip":["a"]}`, `{"zip":["a"]}`, true)
	isFuzzyMatchTestcase(t, `null`, `null`, true)
	isFuzzyMatchTestcase(t, `null`, `1`, false)
	isFuzzyMatchTestcase(t, `{"a":1,"b":2}`, `{"b":2,"a":1}`, true)
	isFuzzyMatchTestcase(t, `123`, `"?? description"`, true)
	isFuzzyMatchTestcase(t, `"123"`, `"?? description"`, true)
	isFuzzyMatchTestcase(t, `"\/[abc]\/"`, `"a"`, true)
	isFuzzyMatchTestcase(t, `"a"`, `"\/[abc]\/"`, true)
}
