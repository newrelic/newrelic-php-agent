<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for an async external request that ends up throwing an uncaught TransferException exception
is marked as http and uri is captured, when using Fibers.
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
      "http.statusCode": 0
    }
  ]
]
*/

/*EXPECT_ERROR_EVENTS 
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "GuzzleHttp\\Exception\\TransferException",
        "error.message": "Uncaught exception 'GuzzleHttp\\Exception\\TransferException' with message 'I'm covered in bees!!!' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "externalDuration": "??",
        "externalCallCount": 1,
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT_REGEX
^\s*Fatal error: Uncaught GuzzleHttp\\Exception\\TransferException: I'm covered in bees!!!
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'GuzzleHttp\\Exception\\TransferException' with message 'I'm covered in bees!!!' in __FILE__:??",
      "GuzzleHttp\\Exception\\TransferException",
      {
        "stack_trace": [
          " in test_uncaught_async_transfer_exception called at ? (?)",
          " in Fiber::start called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      },
      "?? transaction ID"
    ]
  ]
]
*/

function test_uncaught_async_transfer_exception() {
    require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
    require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
    require_guzzle(7);

    $TEST_EXTERNAL_HOST=getenv('TEST_EXTERNAL_HOST');

    $request = new \GuzzleHttp\Psr7\Request('GET', "http://$TEST_EXTERNAL_HOST/resource");

    $stack = GuzzleHttp\HandlerStack::create(
      new GuzzleHttp\Handler\MockHandler([
        new \GuzzleHttp\Exception\TransferException("I'm covered in bees!!!")
      ]));

    $client = new GuzzleHttp\Client([
      'handler' => $stack,

    ]);

    Fiber::suspend();

    // This will cause an uncaught exception
    $promise = $client->sendAsync($request);
    Fiber::suspend();
    $promise->wait();
}

$fiber = new Fiber('test_uncaught_async_transfer_exception');
$fiber->start();
$fiber->resume();
$fiber->resume();
