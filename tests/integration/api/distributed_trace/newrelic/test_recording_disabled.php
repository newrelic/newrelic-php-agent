<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Tests that newrelic_create_distributed_trace_payload() returns an empty object
when recording is stopped.
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
newrelic.distributed_tracing_enabled=1
*/

/*EXPECT
object(newrelic\DistributedTracePayload)#1 (1) {
  ["text":"newrelic\DistributedTracePayload":private]=>
  string(0) ""
}
*/

newrelic_end_transaction();

$emptyObject = newrelic_create_distributed_trace_payload();

var_dump($emptyObject);
