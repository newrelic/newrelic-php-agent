//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

//go:build integration
// +build integration

package newrelic

import (
	"encoding/json"
	"testing"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/collector"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/secrets"
	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/utilization"
)

type CollectorKeys map[string]string

func sampleConnectPayload(lic collector.LicenseKey) *RawConnectPayload {
	info := AppInfo{
		License:       lic,
		Appname:       "Unit Test Application",
		AgentLanguage: "php",
		AgentVersion:  "1.2.3",
		Settings:      nil,
		Environment:   nil,
		HighSecurity:  false,
		Labels:        nil,
	}

	return info.ConnectPayload(utilization.Gather(utilization.Config{
		DetectAWS:        false,
		DetectAzure:      false,
		DetectGCP:        false,
		DetectPCF:        false,
		DetectDocker:     false,
		DetectKubernetes: false,
	}))
}

func sampleErrorData(id AgentRunID) ([]byte, error) {
	d := json.RawMessage(`[1418769.232,"WebTransaction/action/reaction","Unit Test Error Message","Unit Test Error Class",{}]`)

	h := NewErrorHeap(1)
	h.AddError(1, d)

	return h.Data(id, time.Now())
}

func testCommuncation(t *testing.T, license collector.LicenseKey, redirectCollector string, securityToken string, supportedPolicies AgentPolicies) {
	testClient, err := NewClient(&ClientConfig{})
	if nil != err {
		t.Fatal("License: ", license, "Error: ", err)
	}

	var invalidLicense collector.LicenseKey = "invalid_license_key"
	var invalidToken string = "invalid_token"

	args := &ConnectArgs{
		RedirectCollector:            redirectCollector,
		PayloadRaw:                   sampleConnectPayload(license),
		License:                      license,
		Client:                       testClient,
		SecurityPolicyToken:          securityToken,
		AppSupportedSecurityPolicies: supportedPolicies,
	}

	// Invalid Connect
	args.License = invalidLicense
	connectAttempt := ConnectApplication(args)
	if !collector.IsLicenseException(connectAttempt.Err) {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
	}
	if "" != connectAttempt.Collector {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
	}
	if nil != connectAttempt.Reply {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
	}
	args.License = license

	// Test invalid conditions while attempting to connect.
	if "" != args.SecurityPolicyToken {
		// Accounts supporting LASP (Language Agent Security Policy)

		// Invalid Token
		args.SecurityPolicyToken = invalidToken
		connectAttempt := ConnectApplication(args)
		if !collector.IsDisconnect(connectAttempt.Err) {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
		}
		if "" != connectAttempt.Collector {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
		}
		if nil != connectAttempt.Reply {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
		}

		// Empty Token
		args.SecurityPolicyToken = ""
		connectAttempt = ConnectApplication(args)
		if !collector.IsDisconnect(connectAttempt.Err) {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
		}
		if "" != connectAttempt.Collector {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
		}
		if nil != connectAttempt.Reply {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
		}

	} else {
		// Non-LASP accounts

		// Non-empty Token
		args.SecurityPolicyToken = invalidToken
		connectAttempt := ConnectApplication(args)
		if !collector.IsDisconnect(connectAttempt.Err) {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
		}
		if "" != connectAttempt.Collector {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
		}
		if nil != connectAttempt.Reply {
			t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
		}
	}
	args.SecurityPolicyToken = securityToken

	// Successful Connect
	connectAttempt = ConnectApplication(args)
	if nil != connectAttempt.Err {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Err)
	}
	if nil == connectAttempt.Reply {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Reply)
	}
	if "" == connectAttempt.Collector {
		t.Fatal("License: ", license, "Error: ", connectAttempt.Collector)
	}

	reply, err := parseConnectReply(connectAttempt.RawReply)
	if nil != err {
		t.Fatal("License: ", license, "Error: ", err)
	}
	if nil == reply {
		t.Fatal("License: ", license, "Error: ", reply)
	}

	var out []byte
	call := collector.Cmd{
		Collector: connectAttempt.Collector,
		License:   license,
	}

	call.Name = collector.CommandErrors
	call.RunID = string(*reply.ID)
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		data, err := sampleErrorData(*reply.ID)
		if nil != err {
			t.Fatal(err)
		}
		return data, nil
	})

	// Invalid Error Command
	call.License = invalidLicense
	out, err = testClient.Execute(call)
	if nil != out {
		t.Fatal(out)
	}
	if collector.ErrUnauthorized != err {
		t.Fatal(err)
	}

	// Malformed Error Command
	call.License = license
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		return []byte("{"), nil
	})

	out, err = testClient.Execute(call)
	if nil != err {
		t.Fatal(err)
	}
	if string("null") != string(out) {
		t.Fatal(string(out))
	}

	// Valid Error Command
	call.License = license
	call.Collectible = collector.CollectibleFunc(func(auditVersion bool) ([]byte, error) {
		data, err := sampleErrorData(*reply.ID)
		if nil != err {
			t.Fatal(err)
		}
		return data, nil
	})

	out, err = testClient.Execute(call)
	if nil != err {
		t.Fatal(err)
	}
	if string("null") != string(out) {
		t.Fatal(string(out))
	}
}

/*
 * Initiate a connection attempt via ConnectApplication().
 * Expectations: Valid license key and valid token. Expect the daemon to reject the connection attempt.
 * Checks are performed to confirm the rejection came from the daemon and not the server.
 */
func testExpectDaemonDisconnect(t *testing.T, license collector.LicenseKey, redirectCollector string, securityToken string, supportedPolicies AgentPolicies) {
	testClient, err := NewClient(&ClientConfig{})
	if nil != err {
		t.Fatal(err)
	}

	args := &ConnectArgs{
		RedirectCollector:            redirectCollector,
		PayloadRaw:                   sampleConnectPayload(license),
		License:                      license,
		Client:                       testClient,
		SecurityPolicyToken:          securityToken,
		AppSupportedSecurityPolicies: supportedPolicies,
	}

	connectAttempt := ConnectApplication(args)
	// Confirm err is not a server error
	if collector.IsLicenseException(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if collector.IsDisconnect(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if collector.IsRestartException(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	if collector.IsRuntime(connectAttempt.Err) {
		t.Fatal(connectAttempt.Err)
	}
	// Confirm error is not null (something did go wrong)
	if nil == connectAttempt.Err {
		t.Fatal("no connection attempt error found when expected")
	}
	if "" != connectAttempt.Collector {
		t.Fatal(connectAttempt.Collector)
	}
	if nil != connectAttempt.Reply {
		t.Fatal(connectAttempt.Reply)
	}
}

func TestRPM(t *testing.T) {
	var license1 collector.LicenseKey
	var redirectCollector string
	var securityToken string
	var supportedPolicies AgentPolicies
	var jsonCollectorKeys CollectorKeys
	json.Unmarshal([]byte(`{"agent_policies": {"record_sql": {"enabled": true,"supported": true}}}`), &supportedPolicies)

	err := json.Unmarshal([]byte(secrets.NewrelicCollectorKeys), &jsonCollectorKeys)
	if err != nil {
		t.Fatal(err)
		return
	}

	/* Test communication against a non-LASP enabled production account */
	redirectCollector = "collector.newrelic.com"
	license1 = collector.LicenseKey(jsonCollectorKeys["nonLaspProd"])

	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)
	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)

	/* Test communication against a non-LASP enabled staging account */

	redirectCollector = secrets.NewrelicCollectorHost
	license1 = collector.LicenseKey(jsonCollectorKeys["nonLaspStag"])

	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)
	testCommuncation(t, license1, redirectCollector, "", supportedPolicies)

	/* Test communication against a LASP enabled production account */

	// Policies: All policies set to "most-secure" (enabled:false)
	redirectCollector = "collector.newrelic.com"
	license1 = collector.LicenseKey(jsonCollectorKeys["laspProd"])
	securityToken = jsonCollectorKeys["securityToken"]

	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)
	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)

	/* Test communication against LASP enabled staging accounts */

	// Policies: All policies set to "most-secure" (enabled:false)
	redirectCollector = secrets.NewrelicCollectorHost
	license1 = collector.LicenseKey(jsonCollectorKeys["laspStag"])

	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)
	testCommuncation(t, license1, redirectCollector, securityToken, supportedPolicies)

	// Policies: All policies set to "most-secure" (enabled:false), job_arguments has required:true
	redirectCollector = secrets.NewrelicCollectorHost
	license1 = collector.LicenseKey(jsonCollectorKeys["mostSecureStag"])

	testExpectDaemonDisconnect(t, license1, redirectCollector, securityToken, supportedPolicies)
}
