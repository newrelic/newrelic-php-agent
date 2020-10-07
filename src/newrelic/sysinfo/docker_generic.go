//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// +build !linux

package sysinfo

func DockerID() (string, error) {
	return "", ErrFeatureUnsupported
}
