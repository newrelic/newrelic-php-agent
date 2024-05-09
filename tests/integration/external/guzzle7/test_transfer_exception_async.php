<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for an async external request that ends up throwing exception other than BadResponseException
is marked as http and uri is captured. Status code is marked as 0.
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
require_guzzle(7);

$TEST_EXTERNAL_HOST=getenv('TEST_EXTERNAL_HOST');

$request = new \GuzzleHttp\Psr7\Request('GET', "http://$TEST_EXTERNAL_HOST/resource");

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
