//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"bytes"
	"fmt"
	"io/ioutil"
)

// BootID returns the boot ID of the executing kernel.
func BootID() (string, error) {
	data, err := ioutil.ReadFile("/proc/sys/kernel/random/boot_id")
	if err != nil {
		return "", err
	}

	return validateBootID(data)
}

type invalidBootID string

func (e invalidBootID) Error() string {
	return fmt.Sprintf("Boot id has unrecognized format, id=%q", string(e))
}

func isASCIIByte(b byte) bool {
	return (b >= 0x20 && b <= 0x7f)
}

func validateBootID(data []byte) (string, error) {
	// Any ASCII (excluding control characters) string will be sent, up to
	// and including 128 bytes in length.
	trunc := bytes.TrimSpace(data)
	if len(trunc) > 128 {
		trunc = trunc[:128]
	}
	for _, b := range trunc {
		if !isASCIIByte(b) {
			return "", invalidBootID(data)
		}
	}

	return string(trunc), nil
}
