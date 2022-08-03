//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"crypto/sha256"
	"encoding/base64"
	"net/url"
	"regexp"
)

const (
	CommandMetrics      string = "metric_data"
	CommandErrors              = "error_data"
	CommandTraces              = "transaction_sample_data"
	CommandSlowSQLs            = "sql_trace_data"
	CommandCustomEvents        = "custom_event_data"
	CommandErrorEvents         = "error_event_data"
	CommandTxnEvents           = "analytic_event_data"
	CommandConnect             = "connect"
	CommandPreconnect          = "preconnect"
	CommandSpanEvents          = "span_event_data"
	CommandLogEvents           = "log_event_data"
)

const (
	protocolVersion = "17"
)

// LicenseKey represents a license key for an account.
type LicenseKey string

func (cmd *RpmCmd) String() string {
	if cmd.RunID != "" {
		return cmd.Name + " " + cmd.RunID
	}
	return cmd.Name
}

func (cmd *RpmCmd) url(obfuscate bool) string {
	var u url.URL

	u.Host = cmd.Collector
	u.Path = "agent_listener/invoke_raw_method"
	u.Scheme = "https"

	query := url.Values{}
	query.Set("marshal_format", "json")
	query.Set("protocol_version", protocolVersion)
	query.Set("method", cmd.Name)

	if obfuscate {
		query.Set("license_key", cmd.License.String())
	} else {
		query.Set("license_key", string(cmd.License))
	}

	if cmd.RunID != "" {
		query.Set("run_id", cmd.RunID)
	}

	u.RawQuery = query.Encode()
	return u.String()
}

// String obfuscates the license key by removing all but a safe prefix
// and suffix.
func (key LicenseKey) String() string {
	if n := len(key); n > 4 {
		return string(key[0:2] + ".." + key[n-2:])
	}
	return string(key)
}

func (key LicenseKey) Sha256() string {
	sum := sha256.Sum256([]byte(key))
	return base64.StdEncoding.EncodeToString(sum[:])
}

var (
	preconnectHostDefault        = "collector.newrelic.com"
	preconnectRegionLicenseRegex = regexp.MustCompile(`(^.+?)x`)
)

func CalculatePreconnectHost(license LicenseKey, overrideHost string) string {
	if "" != overrideHost {
		return overrideHost
	}
	m := preconnectRegionLicenseRegex.FindStringSubmatch(string(license))
	if len(m) > 1 {
		return "collector." + m[1] + ".nr-data.net"
	}
	return preconnectHostDefault
}
