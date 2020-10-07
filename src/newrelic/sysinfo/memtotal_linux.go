//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"bufio"
	"errors"
	"io"
	"os"
	"regexp"
	"strconv"
)

var memTotalRE = regexp.MustCompile(`^MemTotal:\s+([0-9]+)\s+[kK]B$`)

// PhysicalMemoryBytes the total amount of memory on a system in bytes.
func PhysicalMemoryBytes() (uint64, error) {
	f, err := os.Open("/proc/meminfo")
	if err != nil {
		return 0, err
	}
	defer f.Close()

	return physicalMemoryParse(f)
}

func physicalMemoryParse(f io.Reader) (uint64, error) {
	var err error
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		if m := memTotalRE.FindSubmatch(scanner.Bytes()); m != nil {
			kb, err := strconv.ParseUint(string(m[1]), 10, 64)
			if err != nil {
				return 0, err
			}
			// `/proc/meminfo` has a memory total in kilobytes. Since memory units that
			// don't come in multiples of 1024kb are extremely rare if not nonexistent,
			// we'll multiply by 1024 and call it an exact byte total.
			return kb * 1024, nil
		}
	}

	err = scanner.Err()
	if err == nil {
		err = errors.New("supported MemTotal not found in /proc/meminfo")
	}
	return 0, err
}
