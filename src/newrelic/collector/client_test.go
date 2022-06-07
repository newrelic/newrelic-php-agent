//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"testing"

	"newrelic/version"
)

func TestParseProxy(t *testing.T) {
	testCases := []struct {
		proxy, scheme, host string
	}{
		{"http://localhost", "http", "localhost"},
		{"http://localhost:8080", "http", "localhost:8080"},
		{"http://user@localhost:8080", "http", "localhost:8080"},
		{"http://user:pass@localhost:8080", "http", "localhost:8080"},
		{"https://localhost", "https", "localhost"},
		{"https://localhost:8080", "https", "localhost:8080"},
		{"https://user@localhost:8080", "https", "localhost:8080"},
		{"https://user:pass@localhost:8080", "https", "localhost:8080"},
		{"localhost", "http", "localhost"},
		{"localhost:8080", "http", "localhost:8080"},
		{"user@localhost:8080", "http", "localhost:8080"},
		{"user:pass@localhost:8080", "http", "localhost:8080"},
	}

	for _, tt := range testCases {
		url, err := parseProxy(tt.proxy)
		if err != nil {
			t.Errorf("parseProxy(%q) = %v", tt.proxy, err)
			continue
		}
		if url.Scheme != tt.scheme || url.Host != tt.host {
			t.Errorf("parseProxy(%q) = {Scheme: %q, Host: %q}\nwant {Scheme: %q, Host: %q}",
				tt.proxy, url.Scheme, url.Host, tt.scheme, tt.host)
		}
	}
}

func TestUserAgent(t *testing.T) {
	for _, tc := range []struct {
		language string
		version  string
		expected string
	}{
		{
			language: "Rust",
			version:  "1.2.3",
			expected: "NewRelic-Rust-Agent/1.2.3",
		},
		{
			language: "php",
			version:  "1.2.3",
			expected: "NewRelic-PHP-Agent/1.2.3",
		},
		{
			language: "c",
			version:  "1.2.3",
			expected: "NewRelic-C-Agent/1.2.3",
		},
		{
			language: "sdk",
			version:  "1.2.3",
			expected: "NewRelic-C-Agent/1.2.3",
		},
		{
			language: "",
			version:  "",
			expected: "NewRelic-Native-Agent/unknown",
		},
	} {
		cmd := &RpmControls{
			AgentLanguage: tc.language,
			AgentVersion:  tc.version,
		}

		expected := tc.expected + " NewRelic-GoDaemon/" + version.Number
		actual := cmd.userAgent()
		if expected != actual {
			t.Errorf("invalid user agent; got=%s want=%s", actual, expected)
		}

		actual = cmd.userAgent()
		if expected != actual {
			t.Errorf("invalid user agent; got=%s want=%s", actual, expected)
		}
	}
}
