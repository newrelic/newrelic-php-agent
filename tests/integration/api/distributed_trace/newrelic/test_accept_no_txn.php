<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that newrelic_accept_distributed_trace_payload returns false when
the transaction has ended.
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
newrelic.distributed_tracing_enabled=1
*/

/*EXPECT
ok - Accept returns false
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');
newrelic_end_transaction();
$payload = '{"v":[0,2],"d":{"ty":"App","ac":"000000","ap":"1111111","id":"332c7b9a18777990","tr":"332c7b9a18777990","pr":1.28674,"sa":true,"ti":1530311294670}}';
$result = tap_equal(false, newrelic_accept_distributed_trace_payload($payload), 'Accept returns false');
