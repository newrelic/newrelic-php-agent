//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package integration

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"unicode"
)

var (
	Directives = map[string]func(*Test, []byte) error{
		"ENVIRONMENT":             parseEnv,
		"HEADERS":                 parseHeaders,
		"SKIPIF":                  parseRawSkipIf,
		"INI":                     parseSettings,
		"CONFIG":                  parseConfig,
		"DESCRIPTION":             parseDescription,
		"EXPECT_ANALYTICS_EVENTS": parseAnalyticEvents,
		"EXPECT_CUSTOM_EVENTS":    parseCustomEvents,
		"EXPECT_ERROR_EVENTS":     parseErrorEvents,
		"EXPECT_SPAN_EVENTS":      parseSpanEvents,
		"EXPECT_SPAN_EVENTS_LIKE": parseSpanEventsLike,
		"EXPECT_LOG_EVENTS":       parseLogEvents,
		"EXPECT_METRICS":          parseMetrics,
		"EXPECT_METRICS_EXIST":    parseMetricsExist,
		"EXPECT":                  parseExpect,
		"EXPECT_REGEX":            parseExpectRegex,
		"EXPECT_SCRUBBED":         parseExpectScrubbed,
		"EXPECT_HARVEST":          parseExpectHarvest,
		"EXPECT_SLOW_SQLS":        parseSlowSQLs,
		"EXPECT_TRACED_ERRORS":    parseTracedErrors,
		"EXPECT_TXN_TRACES":       parseTxnTraces,
		"EXPECT_RESPONSE_HEADERS": parseResponseHeaders,
		"XFAIL":                   parseXFail,
	}
)

func ParseTestFile(name string) *Test {
	test := NewTest(name)
	test.Path, _ = filepath.Abs(name)

	if test.IsC() {
		return parseCTestFile(test)
	} else if test.IsPHP() {
		return parsePHPTestFile(test)
	} else {
		return nil
	}
}

func parsePHPTestFile(test *Test) *Test {
	f, err := os.Open(test.Name)
	if err != nil {
		test.Fatal(err)
		return test
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	buf := make([]byte, 0, 64*1024)
	scanner.Buffer(buf, 1024*1024)
	scanner.Split(splitDirectives)
	for scanner.Scan() {
		p := scanner.Text()

		// First non-word character marks the end of the directive.
		i := strings.IndexFunc(p, func(r rune) bool {
			return r != '_' && !unicode.IsDigit(r) && !unicode.IsLetter(r)
		})

		if i < 1 {
			continue // Ignore comments not starting with a directive.
		}

		keyword := p[:i]
		content := []byte(p[i:])

		parseFn := Directives[keyword]
		if nil == parseFn {
			test.Fatal(fmt.Errorf("directive '%s' not supported", keyword))
			return test
		}

		if err := parseFn(test, content); err != nil {
			test.Fatal(fmt.Errorf("parsing '%s' direction failure: %v", keyword, err))
			return test
		}
	}

	if err := scanner.Err(); err != nil {
		test.Fatal(fmt.Errorf("parsing scanner failure: %v", err))
	}
	return test
}

func parseRawSkipIf(t *Test, content []byte) error {
	trimmed := bytes.TrimSpace(content)
	t.rawSkipIf = make([]byte, len(trimmed))
	copy(t.rawSkipIf, trimmed)
	return nil
}

func parseEnv(t *Test, content []byte) error {
	env := make(map[string]string)

	for _, line := range bytes.Split(content, []byte("\n")) {
		trimmed := bytes.TrimSpace(line)
		if len(trimmed) == 0 {
			continue
		}

		var key, value string

		kv := bytes.SplitN(trimmed, []byte("="), 2)
		key = string(bytes.TrimSpace(kv[0]))
		if len(kv) == 2 {
			value = string(bytes.TrimSpace(kv[1]))
		}

		if key == "" {
			return errors.New("invalid environment variable format")
		}

		env[key] = value
	}

	t.Env = env
	return nil
}

func parseHeaders(t *Test, content []byte) error {
	t.headers = make(http.Header)
	trimmed := bytes.TrimSpace(content)
	hs := strings.Fields(string(trimmed))
	for _, h := range hs {
		// must allow: X-Request-Start=t=1368811467146000
		keyval := strings.SplitN(h, "=", 2)
		if len(keyval) == 2 {
			t.headers.Set(keyval[0], keyval[1])
		}
	}

	return nil
}

func parseResponseHeaders(t *Test, content []byte) error {
	t.expectResponseHeaders = make(http.Header)
	trimmed := bytes.TrimSpace(content)
	hs := strings.Fields(string(trimmed))
	for _, h := range hs {
		keyval := strings.SplitN(h, "=", 2)
		if len(keyval) == 2 {
			t.expectResponseHeaders.Add(keyval[0], keyval[1])
		}
	}

	return nil
}

var errBadSetting = errors.New("settings must have format NAME=VALUE")

func parseSettings(t *Test, content []byte) error {
	trimmed := bytes.TrimSpace(content)
	settings := make(map[string]string)
	scanner := bufio.NewScanner(bytes.NewReader(trimmed))
	delim := []byte("=")

	for scanner.Scan() {
		parts := bytes.SplitN(scanner.Bytes(), delim, 2)
		switch len(parts) {
		case 2:
			key, value := bytes.TrimSpace(parts[0]), bytes.TrimSpace(parts[1])
			if len(key) > 0 {
				settings[string(key)] = string(value)
			} else {
				return errBadSetting
			}
		case 1:
			return errBadSetting
		}
	}

	if err := scanner.Err(); err != nil {
		return err
	}
	t.Settings = settings
	return nil
}

func parseAnalyticEvents(test *Test, content []byte) error {
	test.analyticEvents = content
	return nil
}
func parseCustomEvents(test *Test, content []byte) error {
	test.customEvents = content
	return nil
}
func parseErrorEvents(test *Test, content []byte) error {
	test.errorEvents = content
	return nil
}
func parseSpanEvents(test *Test, content []byte) error {
	test.spanEvents = content
	return nil
}
func parseSpanEventsLike(test *Test, content []byte) error {
	test.spanEventsLike = content
	return nil
}
func parseLogEvents(test *Test, content []byte) error {
	test.logEvents = content
	return nil
}
func parseMetrics(test *Test, content []byte) error {
	test.metrics = content
	return nil
}
func parseMetricsExist(test *Test, content []byte) error {
	test.metricsExist = content
	return nil
}
func parseSlowSQLs(test *Test, content []byte) error {
	test.slowSQLs = content
	return nil
}
func parseTracedErrors(test *Test, content []byte) error {
	test.tracedErrors = content
	return nil
}
func parseTxnTraces(test *Test, content []byte) error {
	test.txnTraces = content
	return nil
}
func parseDescription(test *Test, content []byte) error {
	test.Desc = string(bytes.TrimSpace(content))
	return nil
}
func parseXFail(test *Test, content []byte) error {
	test.Xfail = string(bytes.TrimSpace(content))
	if test.Xfail == "" { // add a default comment incase test is missing a comment after XFAIL directive
		test.Xfail = "expected failure"
	}
	return nil
}
func parseExpect(test *Test, content []byte) error {
	test.expect = content
	return nil
}
func parseExpectRegex(test *Test, content []byte) error {
	test.expectRegex = bytes.TrimSpace(content)
	return nil
}
func parseExpectScrubbed(test *Test, content []byte) error {
	test.expectScrubbed = content
	return nil
}

func parseExpectHarvest(test *Test, content []byte) error {
	if "no" == strings.TrimSpace(string(content)) {
		test.SetExpectHarvest(false)
	}
	return nil
}

func parseConfig(test *Test, content []byte) error {
	test.Config = string(bytes.TrimSpace(content))
	return nil
}

var (
	openComment  = []byte("/*")
	closeComment = []byte("*/")
)

// splitDirectives tokenizes the C-style comments which wrap the
// directives embedded in an integration test.
func splitDirectives(data []byte, atEOF bool) (advance int, token []byte, err error) {
	if atEOF && len(data) == 0 {
		return 0, nil, nil
	}

	openStart := bytes.Index(data, openComment)
	if openStart == -1 {
		// Check if a start delimiter may be straddling the buffer.
		if data[len(data)-1] == '/' {
			return len(data) - 1, nil, nil
		}
		return len(data), nil, nil
	}

	openStop := openStart + len(openComment)
	commentLen := bytes.Index(data[openStop:], closeComment)
	if commentLen == -1 {
		if atEOF {
			return len(data), nil, nil
		}

		// Not at EOF and end comment delimiter not found, advance to the
		// start of the comment and request more data.
		return openStart, nil, nil
	}

	closeStart := openStop + commentLen
	closeStop := closeStart + len(closeComment)

	return closeStop, data[openStop:closeStart], nil
}
