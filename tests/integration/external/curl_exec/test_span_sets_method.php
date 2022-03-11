<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that when a curl handle changes request types the span event methods change
as well.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 5
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
        "name": "External\/127.0.0.1\/all",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
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
        "component": "curl"
      },
      {},
      {
        "http.method": "POST",
        "http.url": "??",
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
        "component": "curl"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
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
        "component": "curl"
      },
      {},
      {
        "http.method": "DELETE",
        "http.url": "??",
        "http.statusCode": 200
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$ch = curl_init($url);
curl_exec($ch);
curl_setopt($ch, CURLOPT_POST,TRUE);
curl_exec($ch);
curl_setopt($ch, CURLOPT_HTTPGET, TRUE);
curl_exec($ch);
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "DELETE");
curl_exec($ch);
curl_close($ch);

