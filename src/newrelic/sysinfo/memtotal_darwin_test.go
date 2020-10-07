//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"os/exec"
	"regexp"
	"strconv"
	"testing"
)

var re = regexp.MustCompile(`hw\.memsize:\s*(\d+)`)

func TestPhysicalMemoryBytes(t *testing.T) {
	mem, err := PhysicalMemoryBytes()
	if err != nil {
		t.Error(err)
	}

	output, err := exec.Command("/usr/sbin/sysctl", "hw.memsize").Output()
	if err != nil {
		t.Fatal(err)
	}

	match := re.FindSubmatch(output)
	if match == nil {
		t.Fatal("memory size not found in sysctl output")
	}

	mem2, err := strconv.ParseUint(string(match[1]), 10, 64)
	if err != nil {
		t.Fatal(err)
	}

	if mem != mem2 {
		t.Errorf("memory totals don't match. We computed %d, but hw.memsize is %d", mem, mem2)
	}
}
