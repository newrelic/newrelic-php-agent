//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import "testing"

func TestLicenseString(t *testing.T) {
	var key LicenseKey

	key = "0123456789012345678901234567890123456789"
	if got := key.String(); got != "01..89" {
		t.Errorf("key.String() = %q, want %q", got, "01..89")
	}

	key = "012"
	if got := key.String(); got != "012" {
		t.Errorf("key.String() = %q, want %q", got, "012")
	}
}
