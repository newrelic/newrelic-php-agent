//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"crypto/x509"
	"io/ioutil"
	"path/filepath"
)

type SystemCertPoolState int

// Constants related to the system certificate pool behaviour.
const (
	// The system certificate pool is missing, and should generate a warning.
	SystemCertPoolMissing SystemCertPoolState = iota

	// The system certificate pool is available.
	SystemCertPoolAvailable

	// The daemon was not built in a way where the system certificate pool is
	// relevant, and we should not warn or generate any supportability metrics.
	SystemCertPoolIgnored
)

var (
	DefaultCertPool *x509.CertPool
	CertPoolState   SystemCertPoolState
)

func newCertPoolFromFiles(files []string) (*x509.CertPool, error) {
	pool := x509.NewCertPool()

	for _, f := range files {
		b, err := ioutil.ReadFile(f)
		if nil != err {
			return nil, err
		}
		pool.AppendCertsFromPEM(b)
	}

	return pool, nil
}

func NewCertPool(cafile, capath string) (*x509.CertPool, error) {
	var files []string
	var err error

	if "" != capath {
		files, err = filepath.Glob(filepath.Join(capath, "*.pem"))
		if nil != err {
			return nil, err
		}
	}

	if "" != cafile {
		files = append(files, cafile)
	}

	pool := x509.NewCertPool()

	for _, f := range files {
		b, err := ioutil.ReadFile(f)
		if nil != err {
			return nil, err
		}
		pool.AppendCertsFromPEM(b)
	}

	return pool, nil
}
