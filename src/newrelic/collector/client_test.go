//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"errors"
	"fmt"
	"io/ioutil"
	"net/http"
	"strings"
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

func TestExecuteWhenMaxPayloadSizeExceeded(t *testing.T) {
	cmdPayload := "dummy-data"
	cmd := RpmCmd{
		MaxPayloadSize: 0,
	}
	cs := RpmControls{
		Collectible: CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return nil, nil
			}
			return []byte(cmdPayload), nil
		}),
	}
	testedFn := fmt.Sprintf("client.Execute(payload: %v, MaxPayloadSize: %v)", cmdPayload, cmd.MaxPayloadSize)
	var wantResponseBody []byte = nil
	wantErr := errors.New("payload size too large:")

	client := clientImpl{
		httpClient: &http.Client{
			Transport: roundTripperFunc(func(r *http.Request) (*http.Response, error) {
				// no http.Response because no HTTP request should be made
				return nil, nil
			}),
		},
	}

	resp := client.Execute(&cmd, cs)
	if resp.Body != nil {
		t.Errorf("%s, got [%v], want [%v]", testedFn, resp.Body, wantResponseBody)
	} else if resp.Err == nil {
		t.Errorf("%s, got [%v], want [%v]", testedFn, resp.Err, wantErr)
	} else if !strings.HasPrefix(resp.Err.Error(), wantErr.Error()) {
		t.Errorf("%s, got [%v], want [%v]", testedFn, resp.Err, wantErr)
	}
}

func TestExecuteWhenMaxPayloadSizeNotExceeded(t *testing.T) {
	cmdPayload := "dummy-data"
	cmd := RpmCmd{
		MaxPayloadSize: 100,
	}
	cs := RpmControls{
		Collectible: CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return nil, nil
			}
			return []byte(cmdPayload), nil
		}),
	}
	testedFn := fmt.Sprintf("client.Execute(payload: %v, MaxPayloadSize: %v)", cmdPayload, cmd.MaxPayloadSize)
	var wantErr error = nil

	client := clientImpl{
		httpClient: &http.Client{
			Transport: roundTripperFunc(func(r *http.Request) (*http.Response, error) {
				return &http.Response{
					StatusCode: 200,
					// perform function calls parseResponse which expects
					// a valid JSON. Providing minimal valid JSON as HTTP
					// response body.
					Body: ioutil.NopCloser(strings.NewReader("{}")),
				}, nil
			}),
		},
	}

	// This test does not test http.Response.Body parsing.
	// This test ensures there's no error if payload does
	// not exceed configured max_payload_size_in_bytes.
	// That's why the body is ignored here.
	resp := client.Execute(&cmd, cs)
	if resp.Err != nil {
		t.Errorf("%s, got [%v], want [%v]", testedFn, resp.Err, wantErr)
	}
}
