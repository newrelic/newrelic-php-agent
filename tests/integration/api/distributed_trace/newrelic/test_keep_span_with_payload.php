<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that a distributed tracing guid is created from the right segment and that
the segment is kept when sampling the segment tree.
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.detail = 1
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.max_segments_cli = 2
newrelic.special.expensive_node_min = 0
*/

/*EXPECT_RESPONSE_HEADERS
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
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
        "name": "Custom\/child",
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
    ]
  ]
]
*/

function grandmother() {
    mother();
}

function mother() {
    child();
}

function child() {
    newrelic_create_distributed_trace_payload();
}

grandmother();
