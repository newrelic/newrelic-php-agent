//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"newrelic.com/daemon/newrelic/log"
)

func (t *Test) IsC() bool {
	_, ext := splitExt(t.Path)

	return ext == ".c" || ext == ".cpp"
}

func makeCommand() string {
	cmd := os.Getenv("MAKE")
	if len(cmd) == 0 {
		cmd = "make"
	}
	return cmd
}

func splitExt(s string) (string, string) {
	ext := filepath.Ext(s)
	base := s[:len(s)-len(ext)]

	return base, ext
}

func compileTestFile(test *Test) error {
	testname, _ := splitExt(test.Path)
	config := strings.Replace(test.Config, "\n", "", -1)
	config = strings.Replace(config, "\"", "\\\"", -1)

	cmd := exec.Command(makeCommand(), testname)
	cmd.Dir = filepath.Dir(test.Path)
	cmd.Env = append(os.Environ(),
		fmt.Sprintf("NEW_RELIC_DAEMON_TESTNAME=%s", test.Name),
		fmt.Sprintf("NEW_RELIC_CONFIG=%s", config))
	cmd.Stderr = os.Stderr

	return cmd.Run()
}

func parseCTestFile(test *Test) *Test {
	// C test files are parsed the same way as PHP test files are. In
	// addition, the file is compiled.
	test = parsePHPTestFile(test)
	if err := compileTestFile(test); err != nil {
		test.Fatal(fmt.Errorf("compilation failure: %v", err))
	}
	return test
}

type CCLI struct {
	CLI
}

func CTx(src Script, env, settings map[string]string, headers http.Header, ctx *Context) (Tx, error) {
	env["NEW_RELIC_DAEMON_SOCKET"] = strings.Trim(settings["newrelic.daemon.port"], "\"")
	env["NEW_RELIC_LICENSE_KEY"] = settings["newrelic.license"]

	path := src.Name()
	base, _ := splitExt(path)

	return &CCLI{
		CLI: CLI{
			Path: base,
			Dir:  filepath.Dir(path),
			Env:  flatten(env),
		},
	}, nil
}

func (tx *CCLI) Execute() (http.Header, []byte, error) {
	header, output, err := tx.CLI.Execute()

	// Clean up
	cleanCmd := exec.Command(makeCommand())
	cleanCmd.Args = append(cleanCmd.Args, fmt.Sprintf("clean_%s", filepath.Base(tx.Path)))
	cleanCmd.Dir = tx.Dir

	log.Debugf("command: %v", cleanCmd)

	cleanCmd.Run()

	return header, output, err
}
