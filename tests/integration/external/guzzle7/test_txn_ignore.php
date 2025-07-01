<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that transaction globals are properly freed when using New Relic API
 */

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
*/

/*EXPECT
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing Customer-Header=found tracing endpoint reached
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
    [{"name":"Supportability/PHP/package/guzzlehttp/guzzle/7/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 6/detected"}, [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/library/Autoloader/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Composer/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure"}, [3, 0, 0, 0, 0, 0]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


?>
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;

function run_test() {
    /* Create URL. */
    $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');
    
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
}

run_test();

newrelic_ignore_transaction(true);
newrelic_start_transaction();

run_test();

newrelic_end_transaction();
