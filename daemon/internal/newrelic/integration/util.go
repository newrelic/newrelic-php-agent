//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bytes"
	"fmt"
	"os"
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
	cmd := exec.Command("php", "-d", "extension="+agent_extension, "-r", "echo phpversion('newrelic');")

	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("Failed to get agent version: %v", err).Error()
	}
	return string(output)
}

func IsOPcacheLoaded(php_executable string) bool {
	fmt.Printf("Checking if OPcache is loaded using %s\n", php_executable)
	cmd := exec.Command(php_executable, "-m")

	output, err := cmd.Output()

	if err != nil {
		fmt.Printf("Failed to check if OPcache is loaded: %v\n", err)
		os.Exit(1)
	}

	// Check if "Zend OPcache" is in the output
	return bytes.Contains(output, []byte("Zend OPcache"))
}

func GetOPCacheModuleLoaded(php, cgi string) map[string]bool {
	result := make(map[string]bool)

	result[php] = IsOPcacheLoaded(php)
	result[cgi] = IsOPcacheLoaded(cgi)	

	fmt.Printf("OPcache default loading status: %+v\n", result)

	return result
}