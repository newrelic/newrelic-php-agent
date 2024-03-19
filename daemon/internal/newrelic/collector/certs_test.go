//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

//go:build collector
// +build collector

package collector

// AUTO-GENERATED - DO NOT EDIT

import (
	"crypto/tls"
	"net"
	"net/http"
	"testing"
	"time"
)

func TestVerifyCollector(t *testing.T) {
	hosts := []string{
		"collector.newrelic.com",
		"staging-collector.newrelic.com",
	}

	client := &http.Client{
		Transport: &http.Transport{
			Proxy: http.ProxyFromEnvironment,
			Dial: (&net.Dialer{
				Timeout:   30 * time.Second,
				KeepAlive: 30 * time.Second,
			}).Dial,
			TLSHandshakeTimeout: 10 * time.Second,
			TLSClientConfig:     &tls.Config{RootCAs: DefaultCertPool},
		},
	}

	for _, host := range hosts {
		resp, err := client.Get("https://" + host + "/status/mongrel")
		if err != nil {
			t.Fatalf("Failed to verify %s: %v", host, err)
		}

		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			t.Errorf("Failed to verify %s: status=%s", resp.Status)
		}
	}
}
