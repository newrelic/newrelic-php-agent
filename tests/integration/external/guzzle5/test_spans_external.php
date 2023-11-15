<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Span events are generated and external span event fields are set correctly.
*/

/*SKIPIF
<?php
require('skipif.inc');

if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ],
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
        "component": "Guzzle 4\/5"
      },
      {},
      {
        "http.url": "??",
        "http.method": "GET",
        "http.statusCode": 200
      }
    ],
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
        "component": "Guzzle 4\/5"
      },
      {},
      {
        "http.url": "??",
        "http.method": "POST",
        "http.statusCode": 200
      }
    ]
]
*/

?>
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(5);

/* Create URL */
$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

/* Use guzzle 5 to make an http request */
use GuzzleHttp\Client;

$client = new Client();
$response = $client->get($url);
echo $response->getBody();

$response = $client->post($url, [
    'headers' => [
        'zip' => '  zap']]);
echo $response->getBody();
