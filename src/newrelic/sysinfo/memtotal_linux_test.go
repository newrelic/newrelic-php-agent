//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"os"
	"regexp"
	"strconv"
	"testing"

	"newrelic.com/daemon/newrelic/crossagent"
)

func TestPhysicalMemoryBytes(t *testing.T) {
	var fileRe = regexp.MustCompile(`meminfo_([0-9]+)MB.txt$`)
	var ignoreFile = regexp.MustCompile(`README\.md$`)

	dir := "proc_meminfo"

	testCases, err := crossagent.ReadDir(dir)
	if err != nil {
		t.Fatal(err)
	}

	for _, testFile := range testCases {
		if ignoreFile.MatchString(testFile) {
			continue
		}

		matches := fileRe.FindStringSubmatch(testFile)

		if matches == nil || len(matches) < 2 {
			t.Errorf("File does not match: %q", testFile)
			continue
		}

		intFound := matches[1]

		expectedMib, err := strconv.Atoi(intFound)
		if err != nil {
			t.Error(err)
			continue
		}

		input, err := os.Open(testFile)
		if err != nil {
			t.Error(err)
			continue
		}
		memory, err := physicalMemoryParse(input)
		input.Close() // defer would require keeping as many filehandles open as there are files.

		memory = memory / (1024 * 1024) // bytes -> mib

		if err != nil {
			t.Error(err)
			continue
		} else if memory != uint64(expectedMib) {
			t.Errorf("expected MiB value %d does not match actual value %d", memory, expectedMib)
			continue
		}
	}
}
