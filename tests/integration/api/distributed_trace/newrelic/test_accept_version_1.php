<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that a version 0.1 payload is accepted.
*/

/*INI
newrelic.distributed_tracing_enabled=1
*/

/*EXPECT
ok - Accepted
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');

$payload = "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"ac\":\"333333\",\"ap\":\"4444444\",\"id\":\"332c7b9a18777990\",\"tr\":\"332c7b9a18777990\",\"pr\":1.28674,\"sa\":true,\"ti\":1530311294670}}";

$result = tap_equal(true, newrelic_accept_distributed_trace_payload($payload), 'Accepted');
