<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for a sync external request that ends up throwing BadResponseException exception
is marked as http, uri and status code are captured, when using Fibers.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
require("skipif.inc");
?>
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 1
newrelic.code_level_metrics.enabled = 0
newrelic.fibers.disabled = false
*/

/*ENVIRONMENT
TEST_EXTERNAL_HOST=example.com
*/

/*EXPECT_METRICS_EXIST
External/ENV[TEST_EXTERNAL_HOST]/all
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "External/ENV[TEST_EXTERNAL_HOST]/all",
      "guid": "??",
      "type": "Span",
      "category": "http",
      "priority": "??",
      "sampled": true,
      "timestamp": "??",
      "parentId": "??",
      "span.kind": "client",
      "component": "Guzzle 6"
    },
    {},
    {
      "http.url": "http://ENV[TEST_EXTERNAL_HOST]/resource",
      "http.method": "GET",
      "http.statusCode": 404
    }
  ]
]
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
all's well that ends well
*/

function test_sync_bad_response() {
    require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
    require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
    require_guzzle(7);

    $TEST_EXTERNAL_HOST=getenv('TEST_EXTERNAL_HOST');

    $request = new \GuzzleHttp\Psr7\Request('GET', "http://$TEST_EXTERNAL_HOST/resource");
    $response = new \GuzzleHttp\Psr7\Response(404, [], "Not found!");

    $stack = GuzzleHttp\HandlerStack::create(
      new GuzzleHttp\Handler\MockHandler([
        new \GuzzleHttp\Exception\BadResponseException(
          "ClientException",
          $request,
          $response
          )
      ]));

    $client = new GuzzleHttp\Client([
      'handler' => $stack,

    ]);

    Fiber::suspend();

    try {
        $response = $client->send($request);
    } catch (\GuzzleHttp\Exception\BadResponseException $e) {
        // Expected exception
    }

    Fiber::suspend();

    echo "all's well that ends well" . PHP_EOL;
}

$fiber = new Fiber('test_sync_bad_response');
$fiber->start();
$fiber->resume();
$fiber->resume();
