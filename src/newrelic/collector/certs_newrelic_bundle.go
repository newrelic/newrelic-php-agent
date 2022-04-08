//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// +build !use_system_certs

package collector

import (
	"crypto/x509"
	"newrelic/log"
)

func init() {
	pool := x509.NewCertPool()
	pool.AppendCertsFromPEM([]byte(nrCABundle))
	DefaultCertPool = pool

	if hasSystemCerts() {
		CertPoolState = SystemCertPoolAvailable
	} else {
		CertPoolState = SystemCertPoolMissing
	}
}

func hasSystemCerts() bool {
	// Missing system certificates can manifest in Go in three ways: as an error
	// returned from x509.SystemCertPool(), the pool pointer being nil, or the
	// pool pointer being valid but containing no certificates.  I'm sure there's
	// some great subtlety involved, but the net effect is the same: it don't
	// work.
	pool, err := x509.SystemCertPool()
	hasCerts := (err == nil && pool != nil && len(pool.Subjects()) > 0)

	if !hasCerts {
		log.Warnf("unable to load the system certificate pool; future versions " +
			"of the PHP agent may not be able to communicate with New Relic unless " +
			"ca-certificates are installed on this host")
	}
	return hasCerts
}
