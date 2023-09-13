//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"reflect"
	"testing"
	"time"

	"newrelic.com/daemon/newrelic/collector"
)

// A MockedAppHarvest comprises two groups of data.  First, the dependencies necessary
// to build out an AppHarvest. Second, the AppHarvest built from said dependencies
// using NewMockedAppHarvest().
type MockedAppHarvest struct {
	*App
	processorHarvestChan chan ProcessorHarvest
	cycleDuration        time.Duration
	ah                   *AppHarvest
}

func (m *MockedAppHarvest) NewMockedAppHarvest() {
	harvest := NewHarvest(time.Now(), collector.NewHarvestLimits(nil))

	m.App.HarvestTrigger = triggerBuilder(HarvestAll, time.Duration(m.cycleDuration))

	m.ah = NewAppHarvest(AgentRunID("1234"), m.App, harvest, m.processorHarvestChan)
}

func TestAppHarvestMessageTransformation(t *testing.T) {
	m := &MockedAppHarvest{
		App: &App{
			info: &AppInfo{},
		},
		processorHarvestChan: make(chan ProcessorHarvest),
		cycleDuration:        1 * time.Minute,
	}

	m.NewMockedAppHarvest()

	expectedEvent := ProcessorHarvest{
		AppHarvest: m.ah,
		ID:         "1234",
		Type:       HarvestAll,
	}

	actualEvent := m.ah.NewProcessorHarvestEvent("1234", HarvestAll)

	if !reflect.DeepEqual(expectedEvent, actualEvent) {
		t.Fatal("The Processor Harvest Event was not created as expected")
	}

	m.ah.Close()
}

func TestAppHarvestTrigger(t *testing.T) {
	m := &MockedAppHarvest{
		App: &App{
			info: &AppInfo{},
		},
		processorHarvestChan: make(chan ProcessorHarvest),
		cycleDuration:        2 * time.Millisecond,
	}

	m.NewMockedAppHarvest()

	_, ok := (<-m.processorHarvestChan)

	if !ok {
		t.Fatal("AppHarvest trigger not ok")
	}

	m.ah.Close()
}

func TestAppHarvestClose(t *testing.T) {
	m := &MockedAppHarvest{
		App: &App{
			info: &AppInfo{},
		},
		processorHarvestChan: make(chan ProcessorHarvest),
		cycleDuration:        1 * time.Minute,
	}

	m.NewMockedAppHarvest()

	m.ah.Close()

	// Attempt to read from the AppHarvest's HarvestChannel, trigger
	_, ok := (<-m.ah.trigger)

	if ok {
		t.Fatal("AppHarvest not closed")
	}

	// Attempt to read from the AppHarvest's bool channel, cancel
	_, ok = (<-m.ah.cancel)

	if ok {
		t.Fatal("AppHarvest not closed")
	}
}
