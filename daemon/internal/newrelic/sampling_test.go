//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import "testing"

func TestLowestPriority(t *testing.T) {

	var a, b SamplingPriority

	a = 0.90000
	b = 0.80000

	if a.IsLowerPriority(b) {
		t.Error("a should be higher priority than b")
	}

	if !b.IsLowerPriority(a) {
		t.Error("b should be lower priority than a")
	}

	if b.IsLowerPriority(b) {
		t.Error("b is equal priority to b")
	}
}
