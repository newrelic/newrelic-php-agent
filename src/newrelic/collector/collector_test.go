//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"io/ioutil"
	"net/http"
	"net/url"
	"newrelic/crossagent"
	"strings"
	"testing"
)

var (
	actualData = "my_data"
)

func TestResponseCodeError(t *testing.T) {
	testcases := []struct {
		code            int
		success         bool
		disconnect      bool
		restart         bool
		saveHarvestData bool
	}{
		// success
		{code: 200, success: true, disconnect: false, restart: false, saveHarvestData: false},
		{code: 202, success: true, disconnect: false, restart: false, saveHarvestData: false},
		// disconnect
		{code: 410, success: false, disconnect: true, restart: false, saveHarvestData: false},
		// restart
		{code: 401, success: false, disconnect: false, restart: true, saveHarvestData: false},
		{code: 409, success: false, disconnect: false, restart: true, saveHarvestData: false},
		// save data
		{code: 408, success: false, disconnect: false, restart: false, saveHarvestData: true},
		{code: 429, success: false, disconnect: false, restart: false, saveHarvestData: true},
		{code: 500, success: false, disconnect: false, restart: false, saveHarvestData: true},
		{code: 503, success: false, disconnect: false, restart: false, saveHarvestData: true},
		// other errors
		{code: 400, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 403, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 404, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 405, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 407, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 411, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 413, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 414, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 415, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 417, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 431, success: false, disconnect: false, restart: false, saveHarvestData: false},
		// unexpected weird codes
		{code: -1, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 1, success: false, disconnect: false, restart: false, saveHarvestData: false},
		{code: 999999, success: false, disconnect: false, restart: false, saveHarvestData: false},
	}
	for _, tc := range testcases {
		resp := newRPMResponse(tc.code)
		if tc.success != (nil == resp.Err) {
			t.Error("error", tc.code, tc.success, resp.Err)
		}
		if tc.disconnect != resp.IsDisconnect() {
			t.Error("disconnect", tc.code, tc.disconnect, resp.Err)
		}
		if tc.restart != resp.IsRestartException() {
			t.Error("restart", tc.code, tc.restart, resp.Err)
		}
		if tc.saveHarvestData != resp.ShouldSaveHarvestData() {
			t.Error("save harvest data", tc.code, tc.saveHarvestData, resp.Err)
		}
	}
}

type roundTripperFunc func(*http.Request) (*http.Response, error)

func (fn roundTripperFunc) RoundTrip(r *http.Request) (*http.Response, error) {
	return fn(r)
}

func TestCollectorRequest(t *testing.T) {
	cmd := RpmCmd{
		Name:              "cmd_name",
		Collector:         "collector.com",
		RunID:             "run_id",
		Data:              nil,
		License:           "the_license",
		RequestHeadersMap: map[string]string{"zip": "zap"},
	}
	testField := func(name, v1, v2 string) {
		if v1 != v2 {
			t.Errorf("Field %s want %s, got %s", name, v2, v1)
		}
	}
	cs := RpmControls{
		Collectible: CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return nil, nil
			}
			return []byte(actualData), nil
		}),
		AgentVersion: "agent_version",
	}
	client := clientImpl{
		httpClient: &http.Client{
			Transport: roundTripperFunc(func(r *http.Request) (*http.Response, error) {
				testField("method", r.Method, "POST")
				testField("url", r.URL.String(), "https://collector.com/agent_listener/invoke_raw_method?license_key=the_license&marshal_format=json&method=cmd_name&protocol_version=17&run_id=run_id")
				testField("Accept-Encoding", r.Header.Get("Accept-Encoding"), "identity, deflate")
				testField("Content-Type", r.Header.Get("Content-Type"), "application/octet-stream")
				testField("Content-Encoding", r.Header.Get("Content-Encoding"), "deflate")
				testField("zip", r.Header.Get("zip"), "zap")
				return &http.Response{
					StatusCode: 200,
					Body:       ioutil.NopCloser(strings.NewReader("{\"Body\": 0}")),
				}, nil
			}),
		},
	}
	resp := client.Execute(cmd, cs)
	if nil != resp.Err {
		t.Error(resp.Err)
	}
}

func TestCollectorBadRequest(t *testing.T) {
	cmd := RpmCmd{
		Name:      "cmd_name",
		Collector: "collector.com",
		RunID:     "run_id",
		Data:      nil,
		License:   "the_license",
	}
	cs := RpmControls{
		Collectible: CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return nil, nil
			}
			return []byte(actualData), nil
		}),
		AgentVersion: "agent_version",
	}
	client := clientImpl{
		httpClient: &http.Client{
			Transport: roundTripperFunc(func(r *http.Request) (*http.Response, error) {
				return &http.Response{
					StatusCode: 200,
					Body:       ioutil.NopCloser(strings.NewReader("body")),
				}, nil
			}),
		},
	}
	u := ":" // bad url
	resp := client.perform(u, cmd, cs)
	if nil == resp.Err {
		t.Error("missing expected error")
	}
}

func TestObfuscateLicense(t *testing.T) {
	cmd := RpmCmd{
		Name:      "foo_method",
		Collector: "example.com",
		License:   "123abc",
	}

	obfuscated := cmd.url(true)
	u, err := url.Parse(obfuscated)
	if err != nil {
		t.Fatalf("url.Parse(%q) = %q", obfuscated, err)
	}

	want := "12..bc"
	got := u.Query().Get("license_key")

	if got != want {
		t.Errorf("license key not obfuscated, got=%q want=%q", got, want)
	}
}

func TestObfuscateLicenseShort(t *testing.T) {
	cmd := RpmCmd{
		Name:      "foo_method",
		Collector: "example.com",
		License:   "abc",
	}

	obfuscated := cmd.url(true)
	u, err := url.Parse(obfuscated)
	if err != nil {
		t.Fatalf("url.Parse(%q) = %q", obfuscated, err)
	}

	if got := u.Query().Get("license_key"); got != string(cmd.License) {
		t.Errorf("license key not obfuscated, got=%q want=%q",
			got, cmd.License)
	}
}

func TestCalculatePreconnectHost(t *testing.T) {
	// non-region license
	host := CalculatePreconnectHost("0123456789012345678901234567890123456789", "")
	if host != preconnectHostDefault {
		t.Error(host)
	}
	// override present
	override := "other-collector.newrelic.com"
	host = CalculatePreconnectHost("0123456789012345678901234567890123456789", override)
	if host != override {
		t.Error(host)
	}
	// four letter region
	host = CalculatePreconnectHost("eu01xx6789012345678901234567890123456789", "")
	if host != "collector.eu01.nr-data.net" {
		t.Error(host)
	}
	// five letter region
	host = CalculatePreconnectHost("gov01x6789012345678901234567890123456789", "")
	if host != "collector.gov01.nr-data.net" {
		t.Error(host)
	}
	// six letter region
	host = CalculatePreconnectHost("foo001x6789012345678901234567890123456789", "")
	if host != "collector.foo001.nr-data.net" {
		t.Error(host)
	}
	// 'x' delimiter later in license
	host = CalculatePreconnectHost("foo001x67890123456789012345678901x3456789", "")
	if host != "collector.foo001.nr-data.net" {
		t.Error(host)
	}
	// empty license key
	host = CalculatePreconnectHost("", "")
	if host != preconnectHostDefault {
		t.Error(host)
	}
	// collector.newrelic.com override
	override = "collector.newrelic.com"
	host = CalculatePreconnectHost("eu01xx6789012345678901234567890123456789", override)
	if host != override {
		t.Error(host)
	}
}

func TestPreconnectHostCrossAgent(t *testing.T) {
	var testcases []struct {
		Name               string `json:"name"`
		ConfigFileKey      string `json:"config_file_key"`
		EnvKey             string `json:"env_key"`
		ConfigOverrideHost string `json:"config_override_host"`
		EnvOverrideHost    string `json:"env_override_host"`
		ExpectHostname     string `json:"hostname"`
	}
	err := crossagent.ReadJSON("collector_hostname.json", &testcases)
	if err != nil {
		t.Fatal(err)
	}

	for _, tc := range testcases {
		// mimic file/environment precendence of other agents
		configKey := tc.ConfigFileKey
		if "" != tc.EnvKey {
			configKey = tc.EnvKey
		}
		overrideHost := tc.ConfigOverrideHost
		if "" != tc.EnvOverrideHost {
			overrideHost = tc.EnvOverrideHost
		}

		host := CalculatePreconnectHost(LicenseKey(configKey), overrideHost)
		if host != tc.ExpectHostname {
			t.Errorf(`test="%s" got="%s" expected="%s"`, tc.Name, host, tc.ExpectHostname)
		}
	}
}
