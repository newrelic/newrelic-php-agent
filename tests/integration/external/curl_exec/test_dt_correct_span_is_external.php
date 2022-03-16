<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the correct span event is external.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
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
    "events_seen": 2
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
        "http.url": "??",
        "http.method": "GET",
        "http.statusCode": 200
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function one() {
  return two();
}

function two() {
  $url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
  $ch = curl_init($url);
  tap_not_equal(false, curl_exec($ch), "tracing successful");
  curl_close($ch);
}

$result = one();
