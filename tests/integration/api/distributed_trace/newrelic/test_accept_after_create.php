<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Tests that newrelic_accept_distributed_trace_payload is rejected if made during the
same transaction after a call to newrelic_create_distributed_trace_payload.
*/

/*INI
newrelic.distributed_tracing_enabled=1
*/

/*EXPECT
ok - Accept returns false
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');
$payload = newrelic_create_distributed_trace_payload();
$result = tap_equal(false, newrelic_accept_distributed_trace_payload($payload->text()), 'Accept returns false');
