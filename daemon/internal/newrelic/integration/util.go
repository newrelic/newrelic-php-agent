//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"fmt"
	"os/exec"
)

func GetPHPVersion() string {
	cmd := exec.Command("php", "-r", "echo PHP_VERSION;")

	output, err := cmd.Output()
	if err != nil {
		fmt.Printf("Failed to get PHP version: %v\n", err)
		return "failed"
	}

	return string(output)
}

func GetAgentVersion(agent_extension string) string {
	cmd := exec.Command("php", "-d", "extension="+agent_extension, "-r", "echo newrelic_get_agent_version();")

	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("Failed to get agent version: %v", err).Error()
	}
	return string(output)
}
