<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the actually external call is marked as http.
*/

/*SKIPIF
<?php
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');

if (version_compare(phpversion(), '5.5.0', '<=')) {
    die("skip: PHP > 5.5.0 required\n");
}

if (!unpack_guzzle(6)) {
    die("skip: guzzle 6 installation required\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/


/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
    "events_seen": 3
  },
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
        "name": "Custom\/GuzzleHttp\\Client::__construct",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "timestamp": "??"
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
        "component": "Guzzle 6"
      },
      {},
      {
        "http.url": "??",
        "http.method": "GET",
        "http.statusCode": 200
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(6);

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

/* Use guzzle 6 to make an http request. */
use GuzzleHttp\Client;

$client = new Client();
$response = $client->get($url);
echo $response->getBody();
