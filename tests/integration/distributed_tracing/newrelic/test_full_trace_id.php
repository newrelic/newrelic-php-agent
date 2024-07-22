<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
 * tests that setting pad trace id generates a 32 character id
 * instead of 16
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.distributed_tracing.pad_trace_id = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.code_level_metrics.enabled=false
*/

/*EXPECT
ok - padded trace id
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

newrelic_add_custom_tracer('main');
function main()
{
  $metadata = newrelic_get_linking_metadata();
  tap_equal(strlen($metadata['trace.id']), 32, "padded trace id");
}

main();
