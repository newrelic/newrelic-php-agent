//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"bytes"
	"path/filepath"
	"testing"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/crossagent"
)

func TestDockerID(t *testing.T) {
	var testCases []struct {
		File string `json:"filename"`
		ID   string `json:"containerId"`
	}

	dir := "docker_container_id"
	err := crossagent.ReadJSON(filepath.Join(dir, "cases.json"), &testCases)
	if err != nil {
		t.Fatal(err)
	}

	for _, test := range testCases {
		file := filepath.Join(dir, test.File)
		input, err := crossagent.ReadFile(file)
		if err != nil {
			t.Error(err)
			continue
		}

		want := test.ID
		got, err := parseDockerID(bytes.NewReader(input))

		if want == "" { // invalid, expect error
			if err == nil {
				t.Errorf("Expected Docker ID validation to fail for %q, wanted %q, got %q instead", test.File, want, got)
			}
			continue
		}

		if err != nil {
			t.Errorf("parseDockerID(%q) gives error %q; want %q", file, err.Error(), want)
		}
		if got != want {
			t.Errorf("parseDockerID(%q) = %q, want %q", file, got, want)
		}
	}
}

func TestValidation(t *testing.T) {
	err := validateDockerID("")
	if nil == err {
		t.Error("Validation should fail with an empty string.")
	}
	err = validateDockerID("baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1239")
	if nil != err {
		t.Error("Validation should pass with a 64-character hex string.")
	}
	err = validateDockerID("39ffbba")
	if nil == err {
		t.Error("Validation should have failed with short string.")
	}
	err = validateDockerID("z000000000000000000000000000000000000000000000000100000000000000")
	if nil == err {
		t.Error("Validation should have failed with non-hex characters.")
	}
}
