//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"io/ioutil"
	"reflect"
	"testing"
)

type SupportedPoliciesTest struct {
	Comment           string                         `json:"comment"`
	AgentPolicies     map[string]SecurityPolicyAgent `json:"agent_policies"`
	Expected          string                         `json:"expected"`
	ExpectedPreSha256 string                         `json:"expected_pre_sha256"`
}

type IntersectPoliciesTest struct {
	Comment              string                         `json:"comment"`
	AgentPolicies        map[string]SecurityPolicyAgent `json:"agent_policies"`
	PreconnectPolicies   map[string]SecurityPolicy      `json:"preconnect_policies"`
	ExpectedIntersection map[string]SecurityPolicy      `json:"expected_intersection"`
}

type SupportedPolicyTest struct {
	ExpectedPolicyMatch bool                           `json:"expected_policy_match"`
	TestComment         string                         `json:"comment"`
	AgentPolicies       map[string]SecurityPolicyAgent `json:"agent_policies"`
	PreconnectReply     PreconnectReply                `json:"preconnect_reply"`
}

func logJsonOnTestFailure(t *testing.T, jsonPreconnectReply JSONString, agentPolicies JSONString) {
	t.Logf("jsonPreconnectReply: [%v]", string(jsonPreconnectReply))
	t.Logf("agentPolicies: [%v]", string(agentPolicies))
}

func expectErrorCaseVerifySecurityPolicies(t *testing.T, jsonPreconnectReply JSONString, agentPoliciesJson JSONString, testComment string) {
	var preconnectReply = PreconnectReply{}
	json.Unmarshal(jsonPreconnectReply, &preconnectReply)

	policies := AgentPolicies{}
	json.Unmarshal(agentPoliciesJson, &policies.Policies)

	_, err := policies.verifySecurityPolicies(preconnectReply)
	if nil == err {
		t.Fail()
		t.Logf("Expected error from verifySecurityPolicies, but got nil")
		t.Logf("Test Comment: %v", testComment)
		logJsonOnTestFailure(t, jsonPreconnectReply, agentPoliciesJson)
	}
}

func expectPassingCaseVerifySecurityPolicies(t *testing.T, jsonPreconnectReply JSONString, agentPoliciesJson JSONString, testComment string) {
	var preconnectReply = PreconnectReply{}
	json.Unmarshal(jsonPreconnectReply, &preconnectReply)

	policies := AgentPolicies{}
	json.Unmarshal(agentPoliciesJson, &policies.Policies)

	_, err := policies.verifySecurityPolicies(preconnectReply)

	if err != nil {
		t.Fail()
		t.Logf("Expected pass/nil, but got Error: %v", err)
		t.Logf("Test Comment: %v", testComment)
		logJsonOnTestFailure(t, jsonPreconnectReply, agentPoliciesJson)
	}
}

func TestLaspGetSupportedPoliciesHash(t *testing.T) {
	tests := getSupportedPoliciesHashTestData(t)
	for _, testData := range tests {
		// setup the world
		ap := AgentPolicies{Policies: testData.AgentPolicies}
		if ap.getSupportedPoliciesHash() != testData.Expected {
			t.Logf("Expected %v, but got %v", testData.Expected, ap.getSupportedPoliciesHash())
			t.Fail()
		}

		// make sure the comment-y expected_pre_sha256 values actually
		// hash to what they're supposed to hash to
		hasher := sha256.New()
		hasher.Write([]byte(testData.ExpectedPreSha256))
		if hex.EncodeToString(hasher.Sum(nil)) != testData.Expected {
			t.Logf("Expected %v to hash to %v", testData.ExpectedPreSha256, testData.Expected)
			t.Fail()
		}
	}

	//test new object empty values
	ap := AgentPolicies{}
	hasher := sha256.New()
	hasher.Write([]byte(""))
	if ap.getSupportedPoliciesHash() != hex.EncodeToString(hasher.Sum(nil)) {
		t.Logf("Expected sha256 of empty string, but got %v", ap.getSupportedPoliciesHash())
		t.Fail()
	}

}

func TestLaspAddPoliciesToPayload(t *testing.T) {

	tests := getAddPoliciesToPayloadTestData(t)
	for _, testData := range tests {
		// setup the world
		ap := AgentPolicies{Policies: testData.AgentPolicies}
		payload := RawConnectPayload{}

		// call the method under test
		ap.addPoliciesToPayload(testData.PreconnectPolicies, &payload)

		// check that the payload object has been populated with with the expected value
		if !reflect.DeepEqual(testData.ExpectedIntersection, payload.SecurityPolicies) {
			t.Fail()
			t.Logf("AddPoliciesToPayload test case did not produce expected payload: [%v]", testData)
			t.Logf("    %v", testData.Comment)
		}
	}

	//test invalid/empty cases
	ap := AgentPolicies{}
	payload := RawConnectPayload{}
	var preconnectPolicies map[string]SecurityPolicy
	err := ap.addPoliciesToPayload(preconnectPolicies, &payload)
	if nil == err {
		t.Fail()
		t.Log("Expected error from empty agentPolicies and preconnect policy map")
	}
}

func TestLaspVerifySecurityPolicies(t *testing.T) {
	tests := getVerifySecurityPoliciesTestData(t)
	for _, testData := range tests {
		//re-jsonify to the format our two test function expect
		agentPolicies, err := json.Marshal(testData.AgentPolicies)
		if err != nil {
			t.Fail()
			t.Log("Could not Marshal agent_policies JSON")
		}

		jsonPreconnectReply, err := json.Marshal(testData.PreconnectReply)
		if err != nil {
			t.Fail()
			t.Log("Could not Marshal preconnect_reply JSON")
		}

		if false == testData.ExpectedPolicyMatch {
			expectErrorCaseVerifySecurityPolicies(t, jsonPreconnectReply, agentPolicies, testData.TestComment)
		} else {
			expectPassingCaseVerifySecurityPolicies(t, jsonPreconnectReply, agentPolicies, testData.TestComment)
		}
	}

	//test invalid/empty cases
	policies := AgentPolicies{}
	preconnectReply := PreconnectReply{}
	_, err := policies.verifySecurityPolicies(preconnectReply)

	if nil == err {
		t.Fail()
		t.Log("Expected error from empty agentPolicies and preconnect reply")
	}
}

func getSupportedPoliciesHashTestData(t *testing.T) []SupportedPoliciesTest {
	raw, err := ioutil.ReadFile("../../test-data/getSupportedPoliciesHash.json")
	if err != nil {
		t.Fail()
		t.Log("Could not open getSupportedPoliciesHash.json")
	}

	var c []SupportedPoliciesTest
	json.Unmarshal(raw, &c)
	if 0 == len(c) {
		t.Fail()
		t.Logf("Could not load JSON from getSupportedPoliciesHash.json")
	}

	return c
}

func getAddPoliciesToPayloadTestData(t *testing.T) []IntersectPoliciesTest {
	raw, err := ioutil.ReadFile("../../test-data/agentPoliciesAddPoliciesToPayload.json")
	if err != nil {
		t.Fail()
		t.Log("Could not open agentPoliciesAddPoliciesToPayload.json")
	}

	var c []IntersectPoliciesTest
	json.Unmarshal(raw, &c)
	if 0 == len(c) {
		t.Fail()
		t.Logf("Could not load JSON from agentPoliciesAddPoliciesToPayload.json")
	}

	return c
}

func getVerifySecurityPoliciesTestData(t *testing.T) []SupportedPolicyTest {
	raw, err := ioutil.ReadFile("../../test-data/agentPoliciesVerifySecurityPolicies.json")
	if err != nil {
		t.Fail()
		t.Log("Could not open agentPoliciesVerifySecurityPolicies.json")
	}

	var c []SupportedPolicyTest
	json.Unmarshal(raw, &c)

	if 0 == len(c) {
		t.Fail()
		t.Logf("Could not load JSON from agentPoliciesVerifySecurityPolicies.json")
	}
	return c
}
