<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that DT works with guzzle 7.
 */

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing_exclude_newrelic_header = true
 */

/*EXPECT
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                   [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all", 
      "scope":"OtherTransaction/php__FILE__"},            [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 7/detected"}, [1,    0,    0,    0,    0,    0]],
    [{"name":"Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure"},   
                                                          [3,    0,    0,    0,    0,    0]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, 
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},    
							  [3, "??", "??", "??", "??", "??"]]
  ]
]
*/

?>
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

// create URL
$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

//and now use guzzle 7 to make an http request
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