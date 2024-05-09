<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the span for an async external request that times out is marked as http and uri is captured. Status code is marked as 0.
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
External/127.0.0.1/all
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "External\/127.0.0.1\/all",
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
      "http.url": "http://ENV[EXTERNAL_HOST]/delay",
      "http.method": "GET",
      "http.statusCode": 0
    }
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(6);

$client = new GuzzleHttp\Client([
  // Base URI is used with relative requests
  'base_uri' => "$EXTERNAL_HOST",
  // Set a short timeout, shorter than delay duration in the request
  'timeout'  => 0.01,
]);

$promise = $client->getAsync('/delay?duration=100ms');
GuzzleHttp\Promise\Utils::settle($promise)->wait();
