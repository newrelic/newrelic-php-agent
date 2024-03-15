//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"sort"
	"strings"
)

type AgentPolicies struct {
	Policies map[string]SecurityPolicyAgent `json:"agent_policies"`
}

type SecurityPolicyAgent struct {
	Enabled   bool `json:"enabled"`
	Supported bool `json:"supported"`
}

type errPoliciesEmpty string

func (e errPoliciesEmpty) Error() string {
	return fmt.Sprintf("Encountered empty policy maps: %s", string(e))
}

type errRequiredPolicyNotSupported string

func (e errRequiredPolicyNotSupported) Error() string {
	return fmt.Sprintf("The agent received one or more required security policies that "+
		"it does not recognize and will shut down: {%s} Please check if a newer agent "+
		"version supports these policies or contact support", string(e))
}

type errPolicyMissingFromPreconnect string

func (e errPolicyMissingFromPreconnect) Error() string {
	return fmt.Sprintf("The agent did not receive one or more security policies that it "+
		"expected and will "+"shut down: {%s}. Please contact support.", string(e))
}

// checks that the agent supports all the required policies
// also checks that preconnect included all supported policies
func (ap *AgentPolicies) verifySecurityPolicies(preconnectReply PreconnectReply) (bool, error) {

	if 0 == len(ap.Policies) {
		return false, errPoliciesEmpty("Policy Map from agent was empty when verifying")
	}

	if 0 == len(preconnectReply.SecurityPolicies) {
		return false, errPoliciesEmpty("Policy map from preconnect was empty when when verifying")
	}

	// for each required policy name, check that the policy is supported
	for pcPolicyName, pcPolicy := range preconnectReply.SecurityPolicies {
		if !pcPolicy.Required {
			continue
		}

		// We couldn't find the policy from preconnect in our map of agent policies.
		// Therefore, the policy is unsupported
		if _, ok := ap.Policies[pcPolicyName]; !ok {
			return false, errRequiredPolicyNotSupported(fmt.Sprintf("%v", pcPolicyName))
		}

		// We found a policy in our map of agent policies.  Now we check if its supported
		// value is true/false.  If false, the policy is unsupported
		policy := ap.Policies[pcPolicyName]
		if !policy.Supported {
			return false, errRequiredPolicyNotSupported(fmt.Sprintf("%v", pcPolicyName))
		}
	}

	// Finally, we check each agent supported policy, and that check that the preconnect
	// provided policies know about it (either required, or not). If preconnect
	// does not have something for, then something is seriously wrong.
	for key := range ap.Policies {
		if _, ok := preconnectReply.SecurityPolicies[key]; !ok {
			return false, errPolicyMissingFromPreconnect(fmt.Sprintf("%v", key))
		}
	}

	// if we got here everything checks out as OK, and we return true,no-error
	return true, nil
}

// This function will add
//
// 1. The policies the agent supports (from ap.Policies)
// 2. As well as the `enabled` value we intend to apply (from SecurityPolicy param)
//
// to the connect payload.  (the payload, passed by reference)
//
// The "`enabled` value we intend to apply" will be the more secure of the two
// values -- i.e. enabled:false means the feature is not enabled, and
// therefore more secure
//
// IMPORTANT: function modifies payload.SecurityPolicies
func (ap *AgentPolicies) addPoliciesToPayload(preconnectPolicies map[string]SecurityPolicy, payload *RawConnectPayload) error {

	if 0 == len(ap.Policies) {
		return errPoliciesEmpty("Policy Map from agent was empty when adding to payload")
	}

	if 0 == len(preconnectPolicies) {
		return errPoliciesEmpty("Policy map from preconnect was empty when adding to payload")
	}

	payload.SecurityPolicies = make(map[string]SecurityPolicy)
	// loop over the supported security policies
	for name, policy := range ap.Policies {
		if !policy.Supported {
			continue
		}
		enabled := (policy.Enabled && preconnectPolicies[name].Enabled)
		payload.SecurityPolicies[name] = SecurityPolicy{Enabled: enabled}
	}

	return nil
}

// returns a sort list of supported
// agent policies as a hashed string.
func (ap *AgentPolicies) getSupportedPoliciesHash() string {
	policies := make([]string, len(ap.Policies), len(ap.Policies))

	for name, policy := range ap.Policies {
		if !policy.Supported {
			continue
		}
		policies = append(policies, name)
	}

	sort.Strings(policies)

	//hash the string with sha256, and then convert to a hex string
	hasher := sha256.New()
	hasher.Write([]byte(strings.Join(policies, "")))
	return hex.EncodeToString(hasher.Sum(nil))
}
