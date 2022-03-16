<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test to make sure the correct span is marked as external.
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
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
        "name": "??",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "??",
        "span.kind": "client",
        "component": "file_get_contents"
      },
      {},
      {
        "http.url": "??",
        "http.method": "GET",
        "http.statusCode": 0
      }
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "??",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "??",
        "span.kind": "client",
        "component": "file_get_contents"
      },
      {},
      {
        "http.url": "??",
        "http.method": "POST",
        "http.statusCode": 0
      }
    ]
  ]
]
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function nest() {
  $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
  echo file_get_contents ($url);
  $opts = array(
    'http'=>array(
      'method'=>"POST"
    )
  );

  $context = stream_context_create($opts);
  echo file_get_contents($url, false, $context);

}

nest();
