//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"testing"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/crossagent"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

type CrossAgentRulesTestcase struct {
	Testname string           `json:"testname"`
	Rules    *json.RawMessage `json:"rules"`
	Tests    []struct {
		Input    string `json:"input"`
		Expected string `json:"expected"`
	} `json:"tests"`
}

func (tc *CrossAgentRulesTestcase) run(t *testing.T) {
	// This test relies upon negative lookahead assertions which are not implemented
	// in Go's regexp package. We're skipping implementation of this syntax for now.
	if tc.Testname == "saxon's test" {
		return
	}

	rules := NewMetricRulesFromJSON(*tc.Rules)

	for _, x := range tc.Tests {
		res, out := rules.Apply(x.Input)
		if out != x.Expected {
			t.Fatalf("FAIL: %v\tInput=%v res=%v out=%v Expected=%v\n", tc.Testname, x.Input, res, out, x.Expected)
		}
	}
}

func TestMetricRules(t *testing.T) {
	var tcs []CrossAgentRulesTestcase

	err := crossagent.ReadJSON("rules.json", &tcs)
	if err != nil {
		t.Fatal(err)
	}

	for _, tc := range tcs {
		tc.run(t)
	}
}

func TestMetricRuleWithNegativeLookaheadAssertion(t *testing.T) {
	log.Init(log.LogAlways, "stdout") // Avoid error messages
	js := `[{
		"match_expression":"^(?!account|application).*",
		"replacement":"*",
		"ignore":false,
		"eval_order":0,
		"each_segment":true
	}]`
	rules := NewMetricRulesFromJSON([]byte(js))
	if 0 != rules.Len() {
		t.Fatal(rules)
	}
}

func TestApplyNilRules(t *testing.T) {
	var rules MetricRules

	res, s := rules.Apply("hello")
	if "hello" != s {
		t.Fatal(s)
	}
	if RuleResultUnmatched != res {
		t.Fatal(res)
	}
}

func TestAmbiguousReplacement(t *testing.T) {
	log.Init(log.LogAlways, "stdout") // Avoid error messages
	js := `[{
		"match_expression":"(.*)/[^/]*.(bmp|css|gif|ico|jpg|jpeg|js|png)",
		"replacement":"\\\\1/*.\\2",
		"ignore":false,
		"eval_order":0
	}]`
	rules := NewMetricRulesFromJSON([]byte(js))
	if 0 != rules.Len() {
		t.Fatal(rules)
	}
}
