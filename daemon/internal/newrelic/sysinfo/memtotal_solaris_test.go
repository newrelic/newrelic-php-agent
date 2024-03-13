//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"errors"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"testing"
)

var memTotalRE = regexp.MustCompile(`[Mm]emory\s*size:\s*([0-9]+)\s*([a-zA-Z]+)`)

func TestPhysicalMemoryBytes(t *testing.T) {
	prtconf, err := physicalMemory()
	if err != nil {
		t.Errorf("couldn't parse memory from prtconf: %s", err.Error())
	}

	sysconf, err := PhysicalMemoryBytes()
	if err != nil {
		t.Errorf("couldn't parse memory from prtconf: %s", err.Error())
	}

	// Unfortunately, the pagesize*pages calculation we do for getting the total
	// memory, although standard (the JVM, at least, uses this approach), doesn't
	// match up exactly with the number returned by prtconf. So we add a fudge factor.
	if sysconf > prtconf || sysconf < (prtconf-prtconf/20) {
		t.Errorf("prtconf and calculation from sysconf returned more different values than expected for total memory (prtconf: %d, sysconf: %d)", prtconf, sysconf)
	}
}

// Gather memory using prtconf. We don't use this approach normally because
// we can't be sure we can find the executable in isolation such as zones.
func physicalMemory() (uint64, error) {
	var err error

	output, err := exec.Command("/usr/sbin/prtconf").Output()
	if err != nil {
		return 0, err
	}

	m := memTotalRE.FindSubmatch(output)
	if m == nil {
		return 0, errors.New("memory size not found in prtconf output")
	}

	size, err := strconv.ParseUint(string(m[1]), 10, 64)
	if err != nil {
		return 0, err
	}

	switch strings.ToLower(string(m[2])) {

	case "megabytes", "mb":
		return size * 1024 * 1024, nil
	case "kilobytes", "kb":
		return size * 1024, nil
	default:
		return 0, errors.New("couldn't parse memory size in prtconf output")
	}
}
