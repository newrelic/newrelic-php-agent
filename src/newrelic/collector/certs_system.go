//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

//go:build use_system_certs
// +build use_system_certs

package collector

import (
	"crypto/x509"
	"newrelic/log"
)

func init() {
	pool, err := x509.SystemCertPool()
	if err != nil {
		log.Warnf("unable to load the system certificate pool; "+
			"communication with New Relic will likely be unsuccessful: %v", err)
		pool = x509.NewCertPool()
	}

	DefaultCertPool = pool

	// We'll never warn about missing system certificates here, even if they are
	// missing, since we can't communicate with New Relic anyway.
	CertPoolState = SystemCertPoolIgnored
}
