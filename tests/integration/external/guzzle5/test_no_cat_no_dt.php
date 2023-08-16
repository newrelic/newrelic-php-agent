<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that neither CAT nor DT headers are added when CAT and DT are disabled.
*/

/*SKIPIF
<?php
require('skipif.inc');
*/

/*INI
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                             [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                       [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                  [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 4-5/detected"},         [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


?>
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(5);

/* Create URL. */
$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

/* Use guzzle 5 to make an http request. */
use GuzzleHttp\Client;

$client = new Client();
$response = $client->get($url);
echo $response->getBody();

$response = $client->get($url, [
    'headers' => [
        'zip' => 'zap']]);
echo $response->getBody();

$response = $client->get($url, [
    'headers' => [
        'zip' => 'zap',
        CUSTOMER_HEADER => 'zap']]);
echo $response->getBody();
