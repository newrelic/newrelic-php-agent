//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

//go:build !linux
// +build !linux

package sysinfo

import (
	"testing"
)

func TestDockerID(t *testing.T) {
	_, err := DockerID()
	if err != ErrFeatureUnsupported {
		t.Fatalf("Expected error %v; got: %v", ErrFeatureUnsupported, err)
	}
}
