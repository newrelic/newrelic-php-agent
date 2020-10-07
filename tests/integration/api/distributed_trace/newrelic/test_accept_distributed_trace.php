<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that our new API functions are added correctly.
*/

/*EXPECT
ok - newrelic_accept_distributed_trace_payload exists
ok - newrelic_accept_distributed_trace_payload_httpsafe exists
*/


require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');

/* Some simple data to test */
$payload = new stdClass;
$payload->v = array(0,1);
$payload->d = new stdClass;
$payload->d->ty = "App";
$payload->d->ac = 9123;
$payload->d->ap = 51424;
$payload->d->id = "27856f70d3d314b7"; // guid
$payload->d->tr = "3221bf09aa0bcf0d"; // traceId
$payload->d->pr = 0.1234;             // priority
$payload->d->sa = false;              // sampled
$payload->d->ti = 1482959525577;      // time

$payloadJson      = json_encode($payload);
$payloadHttpSafe  = base64_encode($payloadJson);

/* Are the functions defined? */
$functions = array('newrelic_accept_distributed_trace_payload','newrelic_accept_distributed_trace_payload_httpsafe');
foreach($functions as $function) {
    tap_assert(function_exists($function), "$function exists");
}

/* Some basic checks to make sure things don't explode */
newrelic_accept_distributed_trace_payload($payloadJson,'https');
newrelic_accept_distributed_trace_payload_httpsafe($payloadHttpSafe,'https');

/* Same for the single argument version of the call */
newrelic_accept_distributed_trace_payload($payloadJson);
newrelic_accept_distributed_trace_payload_httpsafe($payloadHttpSafe);



