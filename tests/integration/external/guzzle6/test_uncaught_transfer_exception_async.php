<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for a async external request that ends up throwing an uncaught exception
(other than BadResponseException) is marked as http and uri is captured. Additionally,
test that exception is recorded as traced error, error event, and the root span has error
attributes.
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
log_errors=0
display_errors=1
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
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "OtherTransaction/php__FILE__",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "nr.entryPoint": true,
      "transaction.name": "OtherTransaction/php__FILE__"
    },
    {},
    {
      "error.message": "Uncaught exception 'GuzzleHttp\\Exception\\TransferException' with message 'I'm covered in bees!!!' in __FILE__:??",
      "error.class": "GuzzleHttp\\Exception\\TransferException"
    }
  ],
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
        "stack_trace": [],
        "agentAttributes": "??",
        "intrinsics": "??"
      },
      "?? transaction ID"
    ]
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

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(6);

$TEST_EXTERNAL_HOST=getenv('TEST_EXTERNAL_HOST');

$request = new \GuzzleHttp\Psr7\Request('GET', "http://$TEST_EXTERNAL_HOST/resource");

$stack = GuzzleHttp\HandlerStack::create(
  new GuzzleHttp\Handler\MockHandler([
    new \GuzzleHttp\Exception\TransferException("I'm covered in bees!!!")
  ]));

$client = new GuzzleHttp\Client([
  'handler' => $stack,
]);

$promise = $client->sendAsync($request);
$promise->wait();

echo "you should not see this" . PHP_EOL;
