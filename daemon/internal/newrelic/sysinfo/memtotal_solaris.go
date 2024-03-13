//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

/*
#include <unistd.h>
*/
import "C"

// PhysicalMemoryBytes returns the total amount of memory on a system in bytes.
func PhysicalMemoryBytes() (uint64, error) {
	// The function we're calling on Solaris is
	// long sysconf(int name);
	var pages, pagesize C.long
	var err error

	pagesize, err = C.sysconf(C._SC_PAGE_SIZE) // in bytes
	if pagesize < 1 {
		return 0, err
	}
	pages, err = C.sysconf(C._SC_PHYS_PAGES)
	if pages < 1 {
		return 0, err
	}

	totalMem := uint64(pages) * uint64(pagesize)

	return totalMem, nil
}
