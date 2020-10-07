//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"syscall"
	"unsafe"
)

// PhysicalMemoryBytes returns the total amount of memory on a system in bytes.
func PhysicalMemoryBytes() (uint64, error) {
	mib := []int32{6 /* CTL_HW */, 24 /* HW_MEMSIZE */}

	buf := make([]byte, 8)
	bufLen := uintptr(8)

	_, _, e1 := syscall.Syscall6(syscall.SYS___SYSCTL,
		uintptr(unsafe.Pointer(&mib[0])), uintptr(len(mib)),
		uintptr(unsafe.Pointer(&buf[0])), uintptr(unsafe.Pointer(&bufLen)),
		uintptr(0), uintptr(0))

	if e1 != 0 {
		return 0, e1
	}

	if bufLen != 8 {
		return 0, syscall.EIO
	}

	return *(*uint64)(unsafe.Pointer(&buf[0])), nil
}
