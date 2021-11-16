//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package main

import (
	"bytes"
	"fmt"
	"io"
	"os"

	"newrelic/integration"
)

var (
  Black   = Color("\033[1;30m%s\033[0m")
  Red     = Color("\033[1;31m%s\033[0m")
  Green   = Color("\033[1;32m%s\033[0m")
  Yellow  = Color("\033[1;33m%s\033[0m")
  Purple  = Color("\033[1;34m%s\033[0m")
  Magenta = Color("\033[1;35m%s\033[0m")
  Teal    = Color("\033[1;36m%s\033[0m")
  White   = Color("\033[1;37m%s\033[0m")
)

var (
  Good = Green
  Warn = Yellow
  Fata = Red
)

type TestRunTotals struct {
	passed  int
	skipped int
	failed  int
	xfail   int
}

func Color(colorString string) func(...interface{}) string {
  sprint := func(args ...interface{}) string {
    return fmt.Sprintf(colorString,
      fmt.Sprint(args...))
  }
  return sprint
}

func (totals *TestRunTotals) Accumulate(test *integration.Test) {
	if test.Skipped {
		totals.skipped++
		return
	}
	if test.Failed {
		if "" != test.Xfail {
			totals.xfail++
		} else {
			totals.failed++
		}
		return
	}
	totals.passed++
}

func tapOutput(tests []*integration.Test) {
	totals := TestRunTotals{}
	for _, test := range tests {
		totals.Accumulate(test)
		name := test.Name

		switch {
		case test.Skipped:
			fmt.Println(Warn("skip -"), name, "#", Warn(test.Err))
		case test.Failed:
			if "" != test.Xfail {
				fmt.Println("xfail -", name)
			} else {
				if test.Err != nil {
					fmt.Println(Fata("FAIL -"), Fata(name), "#", test.Err)
					if len(test.Output) > 0 {
						fmt.Println()
						io.Copy(os.Stdout, bytes.NewReader(test.Output))
						fmt.Println()
					}
				} else {
					fmt.Println(Fata("FAIL -"), Fata(name))
					for _, e := range test.Failures {
						fmt.Println(e)
					}
				}
			}
		default:
			fmt.Printf(Good("pass - "))
			fmt.Printf("%s", name)
			if test.Duration > 0 {
				fmt.Printf(" # time=%vs", test.Duration.Seconds())
			}
			fmt.Printf("\n")
		}
	}
	fmt.Println("#", totals.passed, "passed")
	fmt.Println("#", totals.skipped, "skipped")
    if totals.failed == 0 {
	    fmt.Println("#", Good(totals.failed), Good("failed"))
    } else {
	    fmt.Println("#", Fata(totals.failed), Fata("failed"))
    }
	fmt.Println("#", totals.xfail, "expected fail")
}
