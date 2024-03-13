//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package sysinfo

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"regexp"
)

func DockerID() (string, error) {
	f, err := os.Open("/proc/self/cgroup")
	if err != nil {
		return "", err
	}
	defer f.Close()
	return parseDockerID(f)
}

// parseDockerID reads (normally from /proc/self/cgroup) and parses input to
// find what looks like a cgroup from a Docker container. This is conveniently
// also the hash that represents the container. Returns a 64-character hex
// string or an error.
func parseDockerID(r io.Reader) (string, error) {
	// Each line in the cgroup file consists of three colon delimited fields.
	//   1. hierarchy ID  - we don't care about this
	//   2. subsystems    - comma separated list of cgroup subsystem names
	//   3. control group - control group to which the process belongs
	//
	// Example
	//   5:cpuacct,cpu,cpuset:/daemons

	var id string

	// The DockerID must be a 64-character lowercase hex string
	// be greedy and match anything 64-characters or longer to spot invalid IDs
	compileDockerID := regexp.MustCompile("([0-9a-z]{64,})")

	for scanner := bufio.NewScanner(r); scanner.Scan(); {
		line := scanner.Bytes()
		cols := bytes.SplitN(line, []byte(":"), 3)

		if len(cols) < 3 {
			continue
		}

		// We're only interested in the control group that applies
		// to the cpu subsystem.
		if !isCPUCol(cols[1]) {
			continue
		}

		id = compileDockerID.FindString(string(cols[2]))

		if err := validateDockerID(id); err != nil {
			// We can stop searching at this point, the CPU subsystem should
			// only occur once, and its cgroup is not docker or not a format
			// we accept.
			return "", err
		}
		return id, nil
	}

	return "", ErrIdentifierNotFound
}

func isCPUCol(col []byte) bool {
	// Sometimes we have multiple subsystems in one line:
	// 3:cpuacct,cpu:/system.slice/docker-67f98c9e6188f9c1818672a15dbe46237b6ee7e77f834d40d41c5fb3c2f84a2f.scope
	splitCSV := func(r rune) bool { return r == ',' }
	subsysCPU := []byte("cpu")

	for _, subsys := range bytes.FieldsFunc(col, splitCSV) {
		if bytes.Equal(subsysCPU, subsys) {
			return true
		}
	}
	return false
}

type invalidDockerID string

func (e invalidDockerID) Error() string {
	return fmt.Sprintf("Docker container id has unrecognized format, id=%q", string(e))
}

func isHex(r rune) bool {
	return ('0' <= r && r <= '9') || ('a' <= r && r <= 'f')
}

func validateDockerID(id string) error {
	if len(id) != 64 {
		return invalidDockerID(id)
	}

	for _, c := range id {
		if !isHex(c) {
			return invalidDockerID(id)
		}
	}

	return nil
}
