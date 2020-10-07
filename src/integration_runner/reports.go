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

type TestRunTotals struct {
	passed  int
	skipped int
	failed  int
	xfail   int
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
			fmt.Println("skip -", name, "#", test.Err)
		case test.Failed:
			if "" != test.Xfail {
				fmt.Println("xfail -", name)
			} else {
				if test.Err != nil {
					fmt.Println("not ok -", name, "#", test.Err)
					if len(test.Output) > 0 {
						fmt.Println()
						io.Copy(os.Stdout, bytes.NewReader(test.Output))
						fmt.Println()
					}
				} else {
					fmt.Println("not ok -", name)
					for _, e := range test.Failures {
						fmt.Println(e)
					}
				}
			}
		default:
			fmt.Printf("ok - %s", name)
			if test.Duration > 0 {
				fmt.Printf(" # time=%vs", test.Duration.Seconds())
			}
			fmt.Printf("\n")
		}
	}
	fmt.Println("#", totals.passed, "passed")
	fmt.Println("#", totals.skipped, "skipped")
	fmt.Println("#", totals.failed, "failed")
	fmt.Println("#", totals.xfail, "xfail")
}
