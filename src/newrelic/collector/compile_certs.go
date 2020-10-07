//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// +build ignore

// compile-certs generates the New Relic Root CA Bundle.
package main

import (
	"archive/zip"
	"bytes"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"errors"
	"fmt"
	"go/ast"
	"go/format"
	"go/parser"
	"go/token"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path/filepath"
)

var (
	caRootURL = "https://source.datanerd.us/security/SSL_CA_cert_bundle/archive/master.zip"
)

func init() {
	log.SetFlags(0)
}

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintf(os.Stderr, "Usage: compile-certs <archive>\n")
		os.Exit(1)
	}

	//certArchive, err := download(caRootURL)
	certArchive, err := ioutil.ReadFile(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}

	certs, err := extract(certArchive)
	if err != nil {
		log.Fatal(err)
	}

	err = generateBundle(certs)
	if err != nil {
		log.Fatal(err)
	}

	err = generateTests()
	if err != nil {
		log.Fatal(err)
	}
}

func download(url string) ([]byte, error) {
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, errors.New(resp.Status)
	}
	return ioutil.ReadAll(resp.Body)
}

func extract(data []byte) (string, error) {
	r, err := zip.NewReader(bytes.NewReader(data), int64(len(data)))
	if err != nil {
		return "", err
	}

	certs := &bytes.Buffer{}

	for _, hdr := range r.File {
		if filepath.Ext(hdr.Name) != ".pem" {
			continue
		}

		pemFile, err := hdr.Open()
		if err != nil {
			return "", err
		}

		rawCert, err := ioutil.ReadAll(pemFile)
		if err != nil {
			pemFile.Close()
			return "", err
		}
		pemFile.Close()
		certs.Write(rawCert)
		certs.WriteByte('\n')

		for len(rawCert) > 0 {
			var block *pem.Block

			block, rawCert = pem.Decode(rawCert)
			cert, err := x509.ParseCertificate(block.Bytes)
			if err != nil {
				return "", err
			}

			log.Print(dn(cert.Subject))
		}
	}

	return certs.String(), nil
}

func dn(name pkix.Name) string {
	buf := bytes.Buffer{}

	if len(name.Country) > 0 {
		buf.WriteString("/C=" + name.Country[0])
	}
	if len(name.Organization) > 0 {
		buf.WriteString("/O=" + name.Organization[0])
	}
	if len(name.OrganizationalUnit) > 0 {
		buf.WriteString("/OU=" + name.OrganizationalUnit[0])
	}
	if len(name.CommonName) > 0 {
		buf.WriteString("/CN=" + name.CommonName)
	}

	return buf.String()
}

func generateBundle(certs string) error {
	filename := "certs_newrelic.go"

	fset := token.NewFileSet()
	f, err := parser.ParseFile(fset, filename, certBundleTmpl, parser.ParseComments)
	if err != nil {
		return err
	}

	ast.Inspect(f, func(node ast.Node) bool {
		vs, ok := node.(*ast.ValueSpec)
		if !ok {
			return true
		}

		if vs.Names[0].Name != "nrCABundle" {
			return true
		}

		val := vs.Values[0].(*ast.BasicLit)
		val.Value = "`" + certs + "`"
		return false
	})

	out, err := os.Create(filename)
	if err != nil {
		return err
	}
	defer out.Close()
	return format.Node(out, fset, f)
}

func generateTests() error {
	formatted, err := format.Source([]byte(certTestTmpl))
	if err != nil {
		return err
	}

	return ioutil.WriteFile("certs_test.go", formatted, 0644)
}

var certBundleTmpl = `
// +build use_system_certs

package collector

// AUTO-GENERATED - DO NOT EDIT

// nrCABundle contains the PEM-encoded certificate bundle.
var nrCABundle = ""`

var certTestTmpl = `
// +build collector

package collector

// AUTO-GENERATED - DO NOT EDIT

import (
	"crypto/tls"
	"net/http"
	"testing"
)

func TestVerifyCollector(t *testing.T) {
	hosts := []string{
		"collector.newrelic.com",
		"staging-collector.newrelic.com",
	}

	client := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{
				RootCAs: DefaultCertPool(),
			},
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
`

// Test strategy
// 1) Try connect to collector.newrelic.com
// 2) Try connect to staging-collector.newrelic.com
// 3) Verify each cert can be parsed successfully
