//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"bytes"
	"fmt"
	"time"

	"newrelic/log"
)

// type PHPPackages struct {
// 	data JSONString
// }

// phpPackages represents all detected packages reported by an agent.
type PhpPackages struct {
	numSeen int
	data    JSONString
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
		numSeen: 0,
		data:    nil,
	}

	return p
}

// SetPhpPacakges sets the observed package list.
func (packages *PhpPackages) SetPhpPackages(data []byte) error {

	if nil == packages {
		return fmt.Errorf("packages is nil!")
	}
	if nil != packages.data {
		log.Debugf("SetPhpPackages - data field was not nil |^%s| - overwriting data", packages.data)
	}
	if nil == data {
		return fmt.Errorf("data is nil!")
	}
	packages.numSeen = 1
	packages.data = data

	return nil
}

// AddPhpPackagesFromData observes the PHP packages info from the agent.
func (packages *PhpPackages) AddPhpPackagesFromData(data []byte) error {
	return packages.SetPhpPackages(data)
}

// CollectorJSON marshals events to JSON according to the schema expected
// by the collector.
func (packages *PhpPackages) CollectorJSON(id AgentRunID) ([]byte, error) {
	if nil == packages {
		return []byte(`["Jars",[]]`), nil
	}
	if nil == packages.data || 0 == packages.numSeen {
		return []byte(`["Jars",[]]`), nil
	}

	buf := &bytes.Buffer{}

	estimate := 512
	buf.Grow(estimate)
	buf.WriteByte('[')
	buf.WriteString("\"Jars\",")
	if 0 < packages.numSeen {
		buf.Write(packages.data)
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
