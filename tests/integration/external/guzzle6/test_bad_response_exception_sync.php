<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for a sync external reqest that ends up throwing BadResponseException exception
is marked as http, uri and status code are captured.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 1
newrelic.code_level_metrics.enabled = 0
*/

/*EXPECT_METRICS_EXIST
External/example.com/all
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "External/example.com/all",
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
      "http.url": "http://example.com/resource",
      "http.method": "GET",
      "http.statusCode": 404
    }
  ]
]
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
gotcha!
all's well that ends well
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(6);

$request = new \GuzzleHttp\Psr7\Request('GET', "http://example.com/resource");
$response = new \GuzzleHttp\Psr7\Response(404, [], "Not found!");

$stack = GuzzleHttp\HandlerStack::create(
  new GuzzleHttp\Handler\MockHandler([
    new GuzzleHttp\Exception\BadResponseException(
      "ClientException", 
      $request,
      $response
      )
  ]));

$client = new GuzzleHttp\Client([
  'handler' => $stack,

]);

try {
  $response = $client->send($request);
} catch (GuzzleHttp\Exception\BadResponseException $e) {
  echo "gotcha!". PHP_EOL;
}

echo "all's well that ends well" . PHP_EOL;