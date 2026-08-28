<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that guzzle 7 works when both CAT and DT are disabled, with Fibers.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
?>
*/

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT_SPAN_EVENTS
null
*/

/*EXPECT_ERROR_EVENTS
null 
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS_EXIST
External/127.0.0.1/all, 3
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;

function test_guzzle_no_cat_no_dt() {
    /* Create URL. */
    $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

    $client = new Client();

    $response = $client->get($url);
    echo $response->getBody();
    Fiber::suspend();

    $response = $client->get($url, [
        'headers' => [
            'zip' => 'zap']]);
    echo $response->getBody();
    Fiber::suspend();

    $response = $client->get($url, [
        'headers' => [
            'zip' => 'zap',
            CUSTOMER_HEADER => 'zap']]);
    echo $response->getBody();
}

$fiber = new Fiber('test_guzzle_no_cat_no_dt');
$fiber->start();
$fiber->resume();
$fiber->resume();
