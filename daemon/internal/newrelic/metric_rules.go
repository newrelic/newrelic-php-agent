//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"encoding/json"
	"regexp"

	"sort"
	"strings"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

type MetricRuleResult int

const (
	RuleResultMatched MetricRuleResult = iota
	RuleResultUnmatched
	RuleResultIgnore
)

type MetricRule struct {
	// 'Ignore' indicates if the entire transaction should be discarded if
	// there is a match.  This field is only used by "url_rules" and
	// "transaction_name_rules", not "metric_name_rules".
	Ignore              bool   `json:"ignore"`
	EachSegment         bool   `json:"each_segment"`
	ReplaceAll          bool   `json:"replace_all"`
	Terminate           bool   `json:"terminate_chain"`
	Order               int    `json:"eval_order"`
	OriginalReplacement string `json:"replacement"`
	RawExpr             string `json:"match_expression"`

	// Go's regexp backreferences use '${1}' instead of the Perlish '\1', so
	// we transform the replacement string into the Go syntax and store it here.
	TransformedReplacement string
	re                     *regexp.Regexp
}

type MetricRules []*MetricRule

// Go's regexp backreferences use `${1}` instead of the Perlish `\1`, so
// we must transform the replacement string.  This is non-trivial: `\1` is
// a backreference but `\\1` is not.  Rather than count the number of back
// slashes preceding the digit, we simply skip rules with tricky replacements.
var (
	transformReplacementAmbiguous   = regexp.MustCompile(`\\\\([0-9]+)`)
	transformReplacementRegex       = regexp.MustCompile(`\\([0-9]+)`)
	transformReplacementReplacement = "$${${1}}"
)

func NewMetricRulesFromJSON(data []byte) MetricRules {
	var raw []*MetricRule

	if err := json.Unmarshal(data, &raw); nil != err {
		return nil
	}

	valid := make(MetricRules, 0, len(raw))

	for _, r := range raw {
		re, err := regexp.Compile("(?i)" + r.RawExpr)
		if err != nil {
			log.Warnf("Unable to compile rule '%s': %s", r.RawExpr, err)
			continue
		}

		if transformReplacementAmbiguous.MatchString(r.OriginalReplacement) {
			log.Warnf("Unable to transform replacement '%s' for rule '%s'",
				r.OriginalReplacement, r.RawExpr)
			continue
		}

		r.re = re
		r.TransformedReplacement = transformReplacementRegex.ReplaceAllString(r.OriginalReplacement,
			transformReplacementReplacement)
		valid = append(valid, r)
	}

	sort.Sort(valid)

	return valid
}

func (rules *MetricRules) UnmarshalJSON(b []byte) (err error) {
	*rules = NewMetricRulesFromJSON(b)
	return nil
}

func (rules MetricRules) Len() int {
	return len(rules)
}

// Rules should be applied in increasing order
func (rules MetricRules) Less(i, j int) bool {
	return rules[i].Order < rules[j].Order
}
func (rules MetricRules) Swap(i, j int) {
	rules[i], rules[j] = rules[j], rules[i]
}

func replaceFirst(re *regexp.Regexp, s string, replacement string) (MetricRuleResult, string) {
	// Note that ReplaceAllStringFunc cannot be used here since it does
	// not replace $1 placeholders.
	loc := re.FindStringIndex(s)
	if nil == loc {
		return RuleResultUnmatched, s
	}
	firstMatch := s[loc[0]:loc[1]]
	firstMatchReplaced := re.ReplaceAllString(firstMatch, replacement)
	return RuleResultMatched, s[0:loc[0]] + firstMatchReplaced + s[loc[1]:]
}

func (r *MetricRule) Apply(s string) (MetricRuleResult, string) {
	// This code attempts to duplicate the logic of axiom.

	if r.Ignore {
		if "" != r.re.FindString(s) {
			return RuleResultIgnore, ""
		}
		return RuleResultUnmatched, s
	}

	if r.ReplaceAll {
		if r.re.MatchString(s) {
			return RuleResultMatched, r.re.ReplaceAllString(s, r.TransformedReplacement)
		}
		return RuleResultUnmatched, s
	} else if r.EachSegment {
		segments := strings.Split(string(s), "/")
		applied := make([]string, len(segments))
		result := RuleResultUnmatched
		for i, segment := range segments {
			var segmentMatched MetricRuleResult
			segmentMatched, applied[i] = replaceFirst(r.re, segment, r.TransformedReplacement)
			if segmentMatched == RuleResultMatched {
				result = RuleResultMatched
			}
		}
		return result, strings.Join(applied, "/")
	} else {
		return replaceFirst(r.re, s, r.TransformedReplacement)
	}
}

func (rules MetricRules) Apply(s string) (MetricRuleResult, string) {
	var res MetricRuleResult
	matched := false

	for _, rule := range rules {
		var out string
		res, out = rule.Apply(s)
		s = out

		if RuleResultIgnore == res {
			return RuleResultIgnore, ""
		}
		if RuleResultMatched == res {
			matched = true
			if rule.Terminate {
				break
			}
		}
	}

	if matched {
		return RuleResultMatched, s
	}
	return RuleResultUnmatched, s
}
