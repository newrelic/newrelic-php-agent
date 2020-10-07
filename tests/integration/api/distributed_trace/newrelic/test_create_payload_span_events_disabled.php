<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_create_distributed_trace_payload should not create a
payload if span_events_enabled=0
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.span_events_enabled=0
newrelic.transaction_events.enabled = 0
*/

/*EXPECT
object(newrelic\DistributedTracePayload)#1 (1) {
  ["text":"newrelic\DistributedTracePayload":private]=>
  string(0) ""
}
*/

$emptyObject = newrelic_create_distributed_trace_payload();

var_dump($emptyObject);
