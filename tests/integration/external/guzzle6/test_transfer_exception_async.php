<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for an async external reqest that ends up throwing exception other than BadResponseException
is marked as http and uri is captured.
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
      "name": "External\/example.com\/all",
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
      "http.statusCode": 0
    }
  ]
]
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
all's well that ends well
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(6);

$request = new \GuzzleHttp\Psr7\Request('GET', "http://example.com/resource");

$stack = GuzzleHttp\HandlerStack::create(
  new GuzzleHttp\Handler\MockHandler([
    new \GuzzleHttp\Exception\TransferException()
  ]));

$client = new GuzzleHttp\Client([
  'handler' => $stack,
]);

$promise = $client->sendAsync($request);
GuzzleHttp\Promise\Utils::settle($promise)->wait();

echo "all's well that ends well" . PHP_EOL;
