//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import "syscall"

func getFileLimits() (soft, hard uint64, err error) {
	var rl syscall.Rlimit

	err = syscall.Getrlimit(syscall.RLIMIT_NOFILE, &rl)
	if err != nil {
		return 0, 0, err
	}

	return uint64(rl.Cur), uint64(rl.Max), nil
}

func setFileLimits(soft, hard uint64) error {
	rl := syscall.Rlimit{Cur: int64(soft), Max: int64(hard)}
	return syscall.Setrlimit(syscall.RLIMIT_NOFILE, &rl)
}
