//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"encoding/json"
	"fmt"
	"reflect"
	"regexp"
	"strings"
)

var (
	WilcardLiteral = "??"
)

func isWildcard(x interface{}) bool {
	str, ok := x.(string)
	if ok && strings.HasPrefix(str, WilcardLiteral) {
		return true
	}
	return false
}

func isRegex(s string) bool {
	if strings.HasPrefix(s, `/`) && strings.HasSuffix(s, `/`) {
		return true
	}
	return false
}

func isFuzzyMatchRegex(match, str string) error {
	match = strings.TrimSuffix(strings.TrimPrefix(match, `/`), `/`)

	re, err := regexp.Compile(match)
	if nil != err {
		return fmt.Errorf("unable to compile regex %v: %v", match, err)
	}

	if re.MatchString(str) {
		return nil
	}
	return fmt.Errorf("regex %v does not match %v", match, str)
}

func isFuzzyMatchRecursive(x1, x2 interface{}) error {
	if isWildcard(x1) || isWildcard(x2) {
		return nil
	}

	if reflect.TypeOf(x1) != reflect.TypeOf(x2) {
		return fmt.Errorf("types do not match %T vs %T", x1, x2)
	}

	switch v1 := x1.(type) {
	case nil:
		return nil
	case string:
		v2, _ := x2.(string)
		if isRegex(v1) {
			return isFuzzyMatchRegex(v1, v2)
		}
		if isRegex(v2) {
			return isFuzzyMatchRegex(v2, v1)
		}

		if v1 != v2 {
			return fmt.Errorf("values do not match %v vs %v", v1, v2)
		}
	case float64:
		v2, _ := x2.(float64)
		if v1 != v2 {
			return fmt.Errorf("values do not match %v vs %v", v1, v2)
		}
	case bool:
		v2, _ := x2.(bool)
		if v1 != v2 {
			return fmt.Errorf("values do not match %v vs %v", v1, v2)
		}
	case []interface{}:
		v2, _ := x2.([]interface{})
		if len(v1) != len(v2) {
			return fmt.Errorf("arrays do not have same length %v vs %v", len(v1), len(v2))
		}
		for i, e1 := range v1 {
			if err := isFuzzyMatchRecursive(e1, v2[i]); nil != err {
				return err
			}
		}
	case map[string]interface{}:
		v2, _ := x2.(map[string]interface{})
		if len(v1) != len(v2) {
			return fmt.Errorf("hashes do not have same size %v vs %v", v1, v2)
		}
		for key, e1 := range v1 {
			e2, ok := v2[key]
			if !ok {
				return fmt.Errorf("key '%v' not present in hash %v", key, v2)
			}
			if err := isFuzzyMatchRecursive(e1, e2); nil != err {
				return err
			}
		}
	default:
		return fmt.Errorf("unknown type %T", x1)
	}
	return nil
}

func IsFuzzyMatch(j1, j2 []byte) error {
	var x1 interface{}
	var x2 interface{}

	if err := json.Unmarshal(j1, &x1); nil != err {
		return fmt.Errorf("unable to parse json for fuzzy matching: %v", err)
	}

	if err := json.Unmarshal(j2, &x2); nil != err {
		return fmt.Errorf("unable to parse json for fuzzy matching: %v", err)
	}

	return isFuzzyMatchRecursive(x1, x2)
}

func IsFuzzyMatchString(s1, s2 string) error {
	return IsFuzzyMatch([]byte(s1), []byte(s2))
}
