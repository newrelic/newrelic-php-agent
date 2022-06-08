//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"
	"time"
	"strconv"

	"newrelic/limits"
	"newrelic/sysinfo"
	"newrelic/utilization"
)

func TestConnectPayloadInternal(t *testing.T) {
	ramInitializer := new(uint64)
	*ramInitializer = 1000
	processors := 22
	util := &utilization.Data{
		MetadataVersion:   1,
		LogicalProcessors: &processors,
		RamMiB:            ramInitializer,
	}
	info := &AppInfo{
		License:           "the_license",
		Appname:           "one;two",
		AgentLanguage:     "php",
		AgentVersion:      "0.1",
		HostDisplayName:   "my_awesome_host",
		Settings:          map[string]interface{}{"a": "1", "b": true},
		Environment:       JSONString(`[["b", 2]]`),
		HighSecurity:      false,
		Labels:            JSONString(`[{"label_type":"c","label_value":"d"}]`),
		Metadata:          JSONString(`{"NEW_RELIC_METADATA_ONE":"one","NEW_RELIC_METADATA_TWO":"two"}`),
		RedirectCollector: "collector.newrelic.com",
		Hostname:          "some_host",
	}

	info.AgentEventLimits.SpanEventConfig.Limit = 2323

	expected := &RawConnectPayload{
		Pid:             123,
		Language:        "php",
		Version:         "0.1",
		Host:            "some_host",
		HostDisplayName: "my_awesome_host",
		Settings:        map[string]interface{}{"a": "1", "b": true},
		AppName:         []string{"one", "two"},
		HighSecurity:    false,
		Labels:          JSONString(`[{"label_type":"c","label_value":"d"}]`),
		Metadata:        JSONString(`{"NEW_RELIC_METADATA_ONE":"one","NEW_RELIC_METADATA_TWO":"two"}`),
		Environment:     JSONString(`[["b",2]]`),
		Identifier:      "one;two",
		Util:            util,
	}

	expected.Util.Hostname = info.Hostname

	pid := 123
	b := info.ConnectPayloadInternal(pid, util)

	// Compare the string integer and string portions of the structs
	// TestConnectEncodedJSON will do a full comparison after being encoded to bytes
	if b == nil {
		t.Errorf("the struct is set to nil")
	}
	if b.Pid != expected.Pid {
		t.Errorf("expected: %d\nactual: %d", expected.Pid, b.Pid)
	}
	if b.Language != expected.Language {
		t.Errorf("expected: %s\nactual: %s", expected.Language, b.Language)
	}
	if b.Version != expected.Version {
		t.Errorf("expected: %s\nactual: %s", expected.Version, b.Version)
	}
	if b.Host != expected.Host {
		t.Errorf("expected: %s\nactual: %s", expected.Host, b.Host)
	}
	if b.HostDisplayName != expected.HostDisplayName {
		t.Errorf("expected: %s\nactual: %s", expected.HostDisplayName, b.HostDisplayName)
	}
	if b.Identifier != expected.Identifier {
		t.Errorf("expected: %s\nactual: %s", expected.Identifier, b.Identifier)
	}
	if b.Util.MetadataVersion != expected.Util.MetadataVersion {
		t.Errorf("expected: %d\nactual: %d", expected.Util.MetadataVersion, b.Util.MetadataVersion)
	}
	if b.Util.Hostname != expected.Util.Hostname {
		t.Errorf("expected: %s\nactual: %s", expected.Util.Hostname, b.Util.Hostname)
	}
}

func TestConnectPayloadInternalHostname(t *testing.T) {
	daemonHostName, _ := sysinfo.Hostname()
	agentHostName := "agent-acquired-hostname"
	utilizationHostName := ""

	util := &utilization.Data{}
	info := &AppInfo{}

	// No host name in AppInfo, nil utilization data
	info.Hostname = ""

	b := info.ConnectPayloadInternal(1, nil)

	if b.Host != daemonHostName {
		t.Errorf("expected: %s\nactual: %s", daemonHostName, b.Host)
	}
	if b.Util != nil {
		t.Errorf("expected: %v\nactual: %v", nil, b.Util)
	}

	// No host name in AppInfo, no host name in utilization data
	info.Hostname = ""
	util.Hostname = ""

	b = info.ConnectPayloadInternal(1, util)

	if b.Host != daemonHostName {
		t.Errorf("expected: %s\nactual: %s", daemonHostName, b.Host)
	}
	if b.Util.Hostname != daemonHostName {
		t.Errorf("expected: %s\nactual: %s", daemonHostName, b.Util.Hostname)
	}

	// No host name in AppInfo, host name in utilization data
	//
	// The host name in the utilization hash shouldn't be overwritten.
	info.Hostname = ""
	util.Hostname = utilizationHostName

	b = info.ConnectPayloadInternal(1, util)

	if b.Host != daemonHostName {
		t.Errorf("expected: %s\nactual: %s", daemonHostName, b.Host)
	}
	if b.Util.Hostname != daemonHostName {
		t.Errorf("expected: %s\nactual: %s", daemonHostName, b.Util.Hostname)
	}
	if util.Hostname != utilizationHostName {
		t.Errorf("expected: %s\nactual: %s", utilizationHostName, util.Hostname)
	}

	// Host name in AppInfo, nil utilization data
	info.Hostname = agentHostName

	b = info.ConnectPayloadInternal(1, nil)

	if b.Host != agentHostName {
		t.Errorf("expected: %s\nactual: %s", agentHostName, b.Host)
	}
	if b.Util != nil {
		t.Errorf("expected: %v\nactual: %v", nil, b.Util)
	}

	// Host name in AppInfo, no host name in utilization data
	info.Hostname = agentHostName
	util.Hostname = ""

	b = info.ConnectPayloadInternal(1, util)

	if b.Host != agentHostName {
		t.Errorf("expected: %s\nactual: %s", agentHostName, b.Host)
	}
	if b.Util.Hostname != agentHostName {
		t.Errorf("expected: %s\nactual: %s", agentHostName, b.Util.Hostname)
	}

	// Host name in AppInfo, host name in utilization data
	info.Hostname = agentHostName
	util.Hostname = utilizationHostName

	b = info.ConnectPayloadInternal(1, util)

	if b.Host != agentHostName {
		t.Errorf("expected: %s\nactual: %s", agentHostName, b.Host)
	}
	if b.Util.Hostname != agentHostName {
		t.Errorf("expected: %s\nactual: %s", agentHostName, b.Util.Hostname)
	}
}

func TestPreconnectPayloadEncoded(t *testing.T) {

	preconnectPayload := &RawPreconnectPayload{SecurityPolicyToken: "ffff-eeee-eeee-dddd", HighSecurity: false}
	expected := `[` +
		`{` +
		`"security_policies_token":"ffff-eeee-eeee-dddd",` +
		`"high_security":false` +
		`}` +
		`]`

	b, err := EncodePayload(preconnectPayload)
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}

	// Verify that security_policies_token's omitempty is respected
	preconnectPayloadEmpty := &RawPreconnectPayload{}
	expected = `[` +
		`{` +
		`"high_security":false` +
		`}` +
		`]`

	b, err = EncodePayload(preconnectPayloadEmpty)
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}
}


func TestNeedsConnectAttempt(t *testing.T) {
	var app App

	now := time.Date(2015, time.January, 10, 23, 0, 0, 0, time.UTC)

	app.state = AppStateUnknown
	app.lastConnectAttempt = now.Add(-limits.AppConnectAttemptBackoff)
	if !app.NeedsConnectAttempt(now, limits.AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateUnknown
	app.lastConnectAttempt = now
	if app.NeedsConnectAttempt(now, limits.AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateConnected
	app.lastConnectAttempt = now.Add(-limits.AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, limits.AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateDisconnected
	app.lastConnectAttempt = now.Add(-limits.AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, limits.AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateRestart
	app.lastConnectAttempt = now.Add(-limits.AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, limits.AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}

	app.state = AppStateInvalidLicense
	app.lastConnectAttempt = now.Add(-limits.AppConnectAttemptBackoff)
	if app.NeedsConnectAttempt(now, limits.AppConnectAttemptBackoff) {
		t.Fatal(now, app.lastConnectAttempt, app.state)
	}
}

func TestAppKeyEquals(t *testing.T) {
	info := AppInfo{
		License:           "the_license",
		Appname:           "one;two",
		RedirectCollector: "collector.newrelic.com",
		HighSecurity:      false,
		AgentLanguage:     "agent-language",
		Hostname:          "agent-hostname",
	}

	otherInfo := info

	if info.Key() != otherInfo.Key() {
		t.Errorf("Key for application info must match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.License = "other_license"
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.Appname = "other_appname"
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.RedirectCollector = "other_collector"
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.HighSecurity = true
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.AgentLanguage = "other_language"
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.SupportedSecurityPolicies.Policies = make(map[string]SecurityPolicyAgent)
	otherInfo.SupportedSecurityPolicies.Policies["policy-1"] = SecurityPolicyAgent{true, true}
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.Hostname = "other_hostname"
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.TraceObserverHost = "other_traceobserver"
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}

	otherInfo = info
	otherInfo.TraceObserverPort = 31339
	if info.Key() == otherInfo.Key() {
		t.Errorf("Key for application info must not match: %v and %v", info, otherInfo)
	}
}

func TestConnectPayloadEncoded(t *testing.T) {
	ramInitializer := new(uint64)
	*ramInitializer = 1000
	processors := 22
	util := &utilization.Data{
		MetadataVersion:   1,
		LogicalProcessors: &processors,
		RamMiB:            ramInitializer,
	}
	info := &AppInfo{
		License:           "the_license",
		Appname:           "one;two",
		AgentLanguage:     "php",
		AgentVersion:      "0.1",
		HostDisplayName:   "my_awesome_host",
		Settings:          map[string]interface{}{"a": "1", "b": true},
		Environment:       JSONString(`[["b", 2]]`),
		HighSecurity:      false,
		Labels:            JSONString(`[{"label_type":"c","label_value":"d"}]`),
		Metadata:          JSONString(`{"NEW_RELIC_METADATA_ONE":"one","NEW_RELIC_METADATA_TWO":"two"}`),
		RedirectCollector: "collector.newrelic.com",
		Hostname:          "some_host",
	}

    // A valid span event max samples stored value configured from the agent should
    // propagate through and be sent to the collector
	info.AgentEventLimits.SpanEventConfig.Limit = 2323

	pid := 123
	expected := `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"display_host":"my_awesome_host",` +
		`"settings":{"a":"1","b":true},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"metadata":{"NEW_RELIC_METADATA_ONE":"one","NEW_RELIC_METADATA_TWO":"two"},` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"},` +
		`"event_harvest_config":{"report_period_ms":60000,"harvest_limits":{"error_event_data":100,"analytic_event_data":10000,"custom_event_data":10000,"span_event_data":2323}}` +
		`}` +
		`]`

	b, err := EncodePayload(info.ConnectPayloadInternal(pid, util))
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}

    // An invalid span event max samples stored value configured from the agent should
    // propagate defaults through and be sent to the collector
	info.AgentEventLimits.SpanEventConfig.Limit = 12345

	pid = 123
	expected = `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"display_host":"my_awesome_host",` +
		`"settings":{"a":"1","b":true},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"metadata":{"NEW_RELIC_METADATA_ONE":"one","NEW_RELIC_METADATA_TWO":"two"},` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"},` +
		`"event_harvest_config":{"report_period_ms":60000,"harvest_limits":{"error_event_data":100,"analytic_event_data":10000,"custom_event_data":10000,"span_event_data":`+strconv.Itoa(limits.MaxSpanMaxEvents)+`}}`+
		`}` +
		`]`

	b, err = EncodePayload(info.ConnectPayloadInternal(pid, util))
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}

	// an empty string for the HostDisplayName should not produce JSON
	info.AgentEventLimits.SpanEventConfig.Limit = 1001
	info.HostDisplayName = ""
	expected = `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"settings":{"a":"1","b":true},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"metadata":{"NEW_RELIC_METADATA_ONE":"one","NEW_RELIC_METADATA_TWO":"two"},` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"},` +
		`"event_harvest_config":{"report_period_ms":60000,"harvest_limits":{"error_event_data":100,"analytic_event_data":10000,"custom_event_data":10000,"span_event_data":1001}}` +
		`}` +
		`]`

	b, err = EncodePayload(info.ConnectPayloadInternal(pid, util))
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}


	// an empty JSON for the Metadata should be sent
	info.Metadata = JSONString(`{}`)
	expected = `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"settings":{"a":"1","b":true},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"metadata":{},` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"},` +
		`"event_harvest_config":{"report_period_ms":60000,"harvest_limits":{"error_event_data":100,"analytic_event_data":10000,"custom_event_data":10000,"span_event_data":1001}}` +
		`}` +
		`]`

	b, err = EncodePayload(info.ConnectPayloadInternal(pid, util))
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}

	// a NULL JSON for the Metadata should send an empty JSON
	info.Metadata = nil
	expected = `[` +
		`{` +
		`"pid":123,` +
		`"language":"php",` +
		`"agent_version":"0.1",` +
		`"host":"some_host",` +
		`"settings":{"a":"1","b":true},` +
		`"app_name":["one","two"],` +
		`"high_security":false,` +
		`"labels":[{"label_type":"c","label_value":"d"}],` +
		`"environment":[["b",2]],` +
		`"metadata":{},` +
		`"identifier":"one;two",` +
		`"utilization":{"metadata_version":1,"logical_processors":22,"total_ram_mib":1000,"hostname":"some_host"},` +
		`"event_harvest_config":{"report_period_ms":60000,"harvest_limits":{"error_event_data":100,"analytic_event_data":10000,"custom_event_data":10000,"span_event_data":1001}}` +
		`}` +
		`]`

	b, err = EncodePayload(info.ConnectPayloadInternal(pid, util))
	if err != nil {
		t.Error(err)
	} else if string(b) != expected {
		t.Errorf("expected: %s\nactual: %s", expected, string(b))
	}

}
