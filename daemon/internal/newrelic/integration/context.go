//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"os"
	"strings"
	"time"
)

type Context struct {
	PHP      string            // path to the PHP CLI executable
	CGI      string            // path to the PHP CGI executable
	Valgrind string            // path to the Valgrind executable, or empty if disabled
	Env      map[string]string // environment variables to pass to each test
	Settings map[string]string // settings to pass to each test
	Timeout  time.Duration     // maximum test duration
}

func NewContext(php, cgi string) *Context {
	ctx := &Context{PHP: php, CGI: cgi}

	ctx.Env = make(map[string]string)
	for _, kv := range os.Environ() {
		parts := strings.SplitN(kv, "=", 2)
		if len(parts) == 2 {
			ctx.Env[parts[0]] = parts[1]
		} else {
			ctx.Env[parts[0]] = ""
		}
	}

	return ctx
}
