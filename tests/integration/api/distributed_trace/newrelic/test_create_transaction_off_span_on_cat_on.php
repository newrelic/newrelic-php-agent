<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that when span_events_enabled are enabled,
transaction_events.enabled are disabled and cat is enabled
that a distributed trace payload is still created.
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
newrelic.distributed_tracing_enabled=1
newrelic.span_events_enabled=1
newrelic.transaction_events.enabled=0
newrelic.cross_application_tracer.enabled = true
*/

/*EXPECT
ok - text is not empty
*/
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');

$payload = newrelic_create_distributed_trace_payload();
$text = $payload->Text();

tap_not_equal(0, strlen($text), 'text is not empty');
