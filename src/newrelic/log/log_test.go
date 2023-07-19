//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package log

import "testing"

func TestParseLevel(t *testing.T) {
	var tests = []struct {
		want  Level
		input string
	}{
		{LogAlways, "always"},
		{LogError, "error"},
		{LogWarning, "warning"},
		{LogInfo, "info"},
		{LogInfo, ""},
		{LogDebug, "debug"},
		{LogDebug, "verbose"},
		{LogDebug, "verbosedebug"},
		{LogHealthCheck, "healthcheck"},
	}

	for i := range tests {
		want := tests[i].want
		got, err := parseLevel(tests[i].input)

		if err == nil && got != want {
			t.Errorf("parseLevel(%q) = %v, want=%v", tests[i].input, got, want)
		} else if err != nil {
			t.Errorf("parseLevel(%q) = %q, want=%v", tests[i].input, err.Error(), want)
		}
	}
}

func TestParseBadLevel(t *testing.T) {
	if got, err := parseLevel("junk"); err == nil {
		t.Errorf(`parseLevel("junk") = %q, want error`, got)
	}
}

func TestAxiomCompatibility(t *testing.T) {
	var tests = []struct {
		want  Level
		input string
	}{
		// Default is INFO
		{LogInfo, ""},
		// When the subsystem is not given, or is the subsystem is ALL, the
		// last setting wins.
		{LogInfo, "info"},
		{LogAlways, "info;debug;always"},
		{LogAlways, "*=info;all=debug;always"},
		// When the subsystem is not ALL choose the more verbose level.
		{LogDebug, "warning,autorum=verbose,framework=verbosedebug"},
		// Axiom allows both ',' and ';' as subsystem separators.
		{LogDebug, "info,connector=verbosedebug,daemon=debug"},
		{LogDebug, "info;connector=verbosedebug;daemon=debug"},
	}

	for i := range tests {
		want := tests[i].want
		got, err := parseAxiomLevel(tests[i].input)

		if err == nil && got != want {
			t.Errorf("parseAxiomLevel(%q) = %v, want=%v", tests[i].input, got, want)
		} else if err != nil {
			t.Errorf("parseAxiomLevel(%q) = %q, want=%v", tests[i].input, err.Error(), want)
		}
	}

	if got, err := parseAxiomLevel("junk"); err == nil {
		t.Errorf(`parseAxiomLevel("junk") = %q, want error`, got)
	}
}
