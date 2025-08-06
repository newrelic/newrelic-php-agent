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
)

type PhpPackagesKey struct {
	Name    string
	Version string
}

// PhpPackages represents all detected packages reported by an agent
// for a harvest cycle, as well as the filtered list of packages not
// yet seen by the daemon for the lifecycle of the current daemon
// process to be reported to the backend.
type PhpPackages struct {
	numSeen      int
	data         []PhpPackagesKey
	filteredPkgs []PhpPackagesKey
}

// NumSaved returns the total number PHP packages payloads stored.
// Should always be 0 or 1.  The agent reports all the PHP
// packages as a single JSON string.
func (packages *PhpPackages) NumSaved() float64 {
	return float64(packages.numSeen)
}

// NewPhpPackages returns a new PhpPackages struct.
func NewPhpPackages() *PhpPackages {
	p := &PhpPackages{
		numSeen:      0,
		data:         nil,
		filteredPkgs: nil,
	}

	return p
}

// Filter seen php packages data to avoid sending duplicates
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
func (packages *PhpPackages) Filter(pkgHistory map[PhpPackagesKey]struct{}) {
	if packages == nil || packages.data == nil {
		return
	}

	for _, pkgKey := range packages.data {
		_, ok := pkgHistory[pkgKey]
		if !ok {
			pkgHistory[pkgKey] = struct{}{}
			packages.filteredPkgs = append(packages.filteredPkgs, pkgKey)
		}
	}
}

// AddPhpPackagesFromData observes the PHP packages info from the agent.
func (packages *PhpPackages) AddPhpPackagesFromData(data []byte) error {
	if packages == nil {
		return fmt.Errorf("packages is nil")
	}
	if len(data) == 0 {
		return fmt.Errorf("data is nil")
	}

	var x []any

	err := json.Unmarshal(data, &x)
	if err != nil {
		return fmt.Errorf("failed to unmarshal php package json: %s", err.Error())
	}

	for _, pkgJSON := range x {
		pkg, _ := pkgJSON.([]any)
		if len(pkg) != 3 {
			return fmt.Errorf("invalid php package json structure: %+v", pkg)
		}

		name, ok := pkg[0].(string)
		if !ok || len(name) == 0 {
			return fmt.Errorf("unable to parse package name")
		}

		version, ok := pkg[1].(string)
		if !ok || len(version) == 0 {
			return fmt.Errorf("unable to parse package version")
		}

		packages.data = append(packages.data, PhpPackagesKey{name, version})
	}

	packages.numSeen = 1

	return nil
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
