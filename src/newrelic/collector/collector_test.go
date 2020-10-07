//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"net/url"
	"newrelic/crossagent"
	"testing"
)

var (
	actualData = "my_data"
	call       = Cmd{
		Name:      CommandErrors,
		Collector: "the-collector.com",
		License:   "12345",
		RunID:     "db97531",
		Collectible: CollectibleFunc(func(auditVersion bool) ([]byte, error) {
			if auditVersion {
				return nil, nil
			}
			return []byte(actualData), nil
		}),
	}
)

func TestLicenseInvalid(t *testing.T) {
	r := `{"exception":{"message":"Invalid license key, please contact support@newrelic.com","error_type":"NewRelic::Agent::LicenseException"}}`
	reply, err := parseResponse([]byte(r))
	if reply != nil {
		t.Fatal(string(reply))
	}
	if !IsLicenseException(err) {
		t.Fatal(err)
	}
}

func TestRedirectSuccess(t *testing.T) {
	reply, err := parseResponse([]byte(`{"return_value":"staging-collector-101.newrelic.com"}`))
	if nil != err {
		t.Fatal(err)
	}
	if string(reply) != `"staging-collector-101.newrelic.com"` {
		t.Fatal(string(reply))
	}
}

func TestEmptyHash(t *testing.T) {
	reply, err := parseResponse([]byte(`{}`))
	if nil != err {
		t.Fatal(err)
	}
	if nil != reply {
		t.Fatal(string(reply))
	}
}

func TestReturnValueNull(t *testing.T) {
	reply, err := parseResponse([]byte(`{"return_value":null}`))
	if nil != err {
		t.Fatal(err)
	}
	if "null" != string(reply) {
		t.Fatal(string(reply))
	}
}

func TestReplyNull(t *testing.T) {
	reply, err := parseResponse(nil)
	if nil == err || err.Error() != `unexpected end of JSON input` {
		t.Fatal(err)
	}
	if nil != reply {
		t.Fatal(string(reply))
	}
}

func TestConnectSuccess(t *testing.T) {
	inner := `{"agent_run_id":"599551769342729","product_level":40,"js_agent_file":"","cross_process_id":"17833#31785","collect_errors":true,"url_rules":[{"each_segment":false,"match_expression":".*\\.(ace|arj|ini|txt|udl|plist|css|gif|ico|jpe?g|js|png|swf|woff|caf|aiff|m4v|mpe?g|mp3|mp4|mov)$","eval_order":1000,"replace_all":false,"ignore":false,"terminate_chain":true,"replacement":"\/*.\\1"},{"each_segment":true,"match_expression":"^[0-9][0-9a-f_,.-]*$","eval_order":1001,"replace_all":false,"ignore":false,"terminate_chain":false,"replacement":"*"},{"each_segment":false,"match_expression":"^(.*)\/[0-9][0-9a-f_,-]*\\.([0-9a-z][0-9a-z]*)$","eval_order":1002,"replace_all":false,"ignore":false,"terminate_chain":false,"replacement":"\\1\/.*\\2"}],"messages":[{"message":"Reporting to: https:\/\/staging.newrelic.com\/accounts\/17833\/applications\/31785","level":"INFO"}],"data_report_period":60,"collect_traces":true,"sampling_rate":0,"js_agent_loader":"","encoding_key":"d67afc830dab717fd163bfcb0b8b88423e9a1a3b","apdex_t":0.5,"collect_analytics_events":true,"trusted_account_ids":[17833]}`
	outer := `{"return_value":` + inner + `}`
	reply, err := parseResponse([]byte(outer))
	if nil != err {
		t.Fatal(err)
	}
	if string(reply) != inner {
		t.Fatal(string(reply))
	}
}

func TestClientError(t *testing.T) {
	reply, err := parseResponse([]byte(`{"exception":{"message":"something","error_type":"my_error"}}`))
	if nil == err || err.Error() != "my_error: something" {
		t.Fatal(err)
	}
	if nil != reply {
		t.Fatal(string(reply))
	}
}

func TestForceRestartException(t *testing.T) {
	// NOTE: This string was generated manually, not taken from the actual
	// collector.
	r := `{"exception":{"message":"something","error_type":"NewRelic::Agent::ForceRestartException"}}`
	reply, err := parseResponse([]byte(r))
	if reply != nil {
		t.Fatal(string(reply))
	}
	if !IsRestartException(err) {
		t.Fatal(err)
	}
}

func TestForceDisconnectException(t *testing.T) {
	// NOTE: This string was generated manually, not taken from the actual
	// collector.
	r := `{"exception":{"message":"something","error_type":"NewRelic::Agent::ForceDisconnectException"}}`
	reply, err := parseResponse([]byte(r))
	if reply != nil {
		t.Fatal(string(reply))
	}
	if !IsDisconnect(err) {
		t.Fatal(err)
	}
}

func TestRuntimeError(t *testing.T) {
	// NOTE: This string was generated manually, not taken from the actual
	// collector.
	r := `{"exception":{"message":"something","error_type":"RuntimeError"}}`
	reply, err := parseResponse([]byte(r))
	if reply != nil {
		t.Fatal(string(reply))
	}
	if !IsRuntime(err) {
		t.Fatal(err)
	}
}

func TestUnknownError(t *testing.T) {
	r := `{"exception":{"message":"something","error_type":"unknown_type"}}`
	reply, err := parseResponse([]byte(r))
	if reply != nil {
		t.Fatal(string(reply))
	}
	if nil == err || err.Error() != "unknown_type: something" {
		t.Fatal(err)
	}
}

func TestObfuscateLicense(t *testing.T) {
	cmd := Cmd{
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
	cmd := Cmd{
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
