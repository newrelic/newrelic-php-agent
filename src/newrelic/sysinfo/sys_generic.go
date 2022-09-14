//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

//go:build !linux
// +build !linux

package sysinfo

import "os"

// Hostname returns the host name reported by the kernel.
func Hostname() (string, error) {
	return os.Hostname()
}
