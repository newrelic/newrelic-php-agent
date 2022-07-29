//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package newrelic

import (
	"testing"

	"newrelic/collector"
)

func TestHarvestTriggerGet(t *testing.T) {

	reply := &ConnectReply{}

	errorEventConfig := 100
	txnEventData := 10000
	customEventConfig := 10000
	spanEventConfig := 1000

	reply.EventHarvestConfig = collector.EventHarvestConfig{
		ReportPeriod: 60000,
		EventConfigs: collector.EventConfigs{
			ErrorEventConfig: collector.Event{
				Limit:        errorEventConfig,
				ReportPeriod: 60000,
			},
			AnalyticEventConfig: collector.Event{
				Limit:        txnEventData,
				ReportPeriod: 60000,
			},
			CustomEventConfig: collector.Event{
				Limit:        customEventConfig,
				ReportPeriod: 60000,
			},
			SpanEventConfig: collector.Event{
				Limit:        spanEventConfig,
				ReportPeriod: 60000,
			},
		}}

	trigger := getHarvestTrigger("1234", reply)

	if trigger == nil {
		t.Fatal("No harvest trigger")
	}
}

func TestHarvestTriggerGetCustom(t *testing.T) {

	reply := &ConnectReply{}

	errorEventConfig := 100
	txnEventData := 10000
	customEventConfig := 10000
	spanEventConfig := 1000

	reply.EventHarvestConfig = collector.EventHarvestConfig{
		ReportPeriod: 60000,
		EventConfigs: collector.EventConfigs{
			ErrorEventConfig: collector.Event{
				Limit:        errorEventConfig,
				ReportPeriod: 60000,
			},
			AnalyticEventConfig: collector.Event{
				Limit:        txnEventData,
				ReportPeriod: 60000,
			},
			CustomEventConfig: collector.Event{
				Limit:        customEventConfig,
				ReportPeriod: 60000,
			},
			SpanEventConfig: collector.Event{
				Limit:        spanEventConfig,
				ReportPeriod: 60000,
			},
		}}

	trigger := getHarvestTrigger("1234", reply)

	if trigger == nil {
		t.Fatal("No custom harvest trigger created")
	}

}
