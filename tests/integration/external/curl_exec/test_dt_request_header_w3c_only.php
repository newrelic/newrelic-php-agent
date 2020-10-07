<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
If cross_application_tracer is off and distributed_tracing_enabled is on
a new relic header should be sent when using curl_exec.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing_exclude_newrelic_header = true
*/

/*EXPECT_REGEX
ok - successful
ok - no new relic header
ok - tracestate exists
ok - traceparent exists
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$ch = curl_init();
curl_setopt($ch, CURLOPT_URL, $url);
curl_setopt($ch, CURLOPT_FAILONERROR,1);
curl_setopt($ch, CURLOPT_FOLLOWLOCATION,1);
curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
curl_setopt($ch, CURLOPT_TIMEOUT, 15);
curl_setopt($ch, CURLINFO_HEADER_OUT, true);

tap_not_equal(false, curl_exec($ch), "successful");
$headers_sent = curl_getinfo($ch, CURLINFO_HEADER_OUT);
tap_not_equal(true, strpos($headers_sent, "newrelic"), "no new relic header");
tap_not_equal(false, strpos($headers_sent, "tracestate"), "tracestate exists");
tap_not_equal(false, strpos($headers_sent, "traceparent"), "traceparent exists");

curl_close($ch);
