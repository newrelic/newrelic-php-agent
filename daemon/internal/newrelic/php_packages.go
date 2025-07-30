//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"encoding/json"
	"fmt"
	"time"

	"github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/log"
)

type PhpPackagesKey struct {
	Name    string
	Version string
}

// phpPackages represents all detected packages reported by an agent.
type PhpPackages struct {
	numSeen      int
	data         []JSONString
	filteredPkgs []PhpPackagesKey
}

// NumSeen returns the total number PHP packages payloads stored.
// Should always be 0 or 1.  The agent reports all the PHP
// packages as a single JSON string.
func (packages *PhpPackages) NumSaved() float64 {
	return float64(packages.numSeen)
}

// newPhpPackages returns a new PhpPackages struct.
func NewPhpPackages() *PhpPackages {
	p := &PhpPackages{
		numSeen:      0,
		data:         nil,
		filteredPkgs: nil,
	}

	return p
}

// filter seen php packages data to avoid sending duplicates
//
// the `App` structure contains a map of PHP Packages the reporting
// application has encountered.
//
// the map of packages should persist for the duration of the
// current connection
//
// the JSON format received from the agent is:
//
//	[["package_name","version",{}],...]
//
// for each entry, assign the package name and version to the `PhpPackagesKey`
// struct and use the key to verify data does not exist in the map. If the
// key does not exist, add it to the map and the slice of filteredPkgs to be
// sent in the current harvest.
func (packages *PhpPackages) FilterData(pkgHistory map[PhpPackagesKey]struct{}) {
	if packages.data == nil {
		return
	}

	var pkgKey PhpPackagesKey
	var x []interface{}

	for i := 0; i < len(packages.data); i++ {
		err := json.Unmarshal(packages.data[i], &x)
		if err != nil {
			log.Errorf("failed to unmarshal php package json: %s", err)
			return
		}

		for _, pkgJson := range x {
			pkg, _ := pkgJson.([]interface{})
			if len(pkg) != 3 {
				log.Errorf("invalid php package json structure: %+v", pkg)
				return
			}
			name, ok := pkg[0].(string)
			version, ok := pkg[1].(string)
			pkgKey = PhpPackagesKey{name, version}
			_, ok = pkgHistory[pkgKey]
			if !ok {
				pkgHistory[pkgKey] = struct{}{}
				packages.filteredPkgs = append(packages.filteredPkgs, pkgKey)
			}
		}
	}
}

// SetPhpPackages sets the observed package list.
func (packages *PhpPackages) SetPhpPackages(data []byte) error {
	if nil == packages {
		return fmt.Errorf("packages is nil!")
	}
	if nil != packages.data {
		log.Debugf("SetPhpPackages - data field was not nil |^%+v| - appending data", packages.data)
	}
	if nil == data {
		return fmt.Errorf("data is nil!")
	}
	packages.numSeen = 1
	packages.data = append(packages.data, data)

	return nil
}

// AddPhpPackagesFromData observes the PHP packages info from the agent.
func (packages *PhpPackages) AddPhpPackagesFromData(data []byte) error {
	return packages.SetPhpPackages(data)
}

// CollectorJSON marshals events to JSON according to the schema expected
// by the collector.
func (packages *PhpPackages) CollectorJSON(id AgentRunID) ([]byte, error) {
	if packages.Empty() {
		return []byte(`["Jars",[]]`), nil
	}

	var buf bytes.Buffer

	estimate := 512
	buf.Grow(estimate)
	buf.WriteByte('[')
	buf.WriteString(`"Jars",`)
	if len(packages.filteredPkgs) > 0 {
		buf.WriteByte('[')
		for _, pkg := range packages.filteredPkgs {
			buf.WriteString(`["`)
			buf.WriteString(pkg.Name)
			buf.WriteString(`","`)
			buf.WriteString(pkg.Version)
			buf.WriteString(`",{}],`)
		}

		// swap last ',' character with ']'
		buf.Truncate(buf.Len() - 1)
		buf.WriteByte(']')
	} else {
		buf.WriteString("[]")
	}
	buf.WriteByte(']')

	return buf.Bytes(), nil
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of events to the collector
// fails. After a failed delivery attempt, package info is currently
// dropped
func (packages *PhpPackages) FailedHarvest(newHarvest *Harvest) {
}

// Empty returns true if the collection is empty.
func (packages *PhpPackages) Empty() bool {
	return nil == packages || nil == packages.data || 0 == packages.numSeen
}

// Data marshals the collection to JSON according to the schema expected
// by the collector.
func (packages *PhpPackages) Data(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return packages.CollectorJSON(id)
}

// Audit marshals the collection to JSON according to the schema
// expected by the audit log. For PHP packages, the audit schema is
// the same as the schema expected by the collector.
func (packages *PhpPackages) Audit(id AgentRunID, harvestStart time.Time) ([]byte, error) {
	return nil, nil
}
