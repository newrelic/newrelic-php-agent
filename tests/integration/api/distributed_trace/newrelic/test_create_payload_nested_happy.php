<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Tests newrelic_create_distributed_trace_payload() on a happy, nested path. We can ensure the payloads were associated with the correct segment by using the INI settings to limit the spans it saves to only those with payloads (and the root).
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.detail = 1
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.max_segments_cli = 3
newrelic.special.expensive_node_min = 0
*/

/*EXPECT_ERROR_EVENTS
null
*/

/*EXPECT_RESPONSE_HEADERS
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
       "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/a",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},      
      {}
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/b",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},      
      {}
    ]
  ]
]
*/

/*EXPECT
Hello
0.2
0
*/

function c()
{
    time_nanosleep(0, 100000000);
    echo fraction(5) . "\n";
    echo fraction(0) . "\n";
}

function b()
{
    time_nanosleep(0, 100000000);
    try {
        c();
    } catch (RuntimeException $e) {
        echo("Caught exception: " . $e->getMessage() . "\n");
    }
    newrelic_create_distributed_trace_payload();
}

function fraction($x) {
    time_nanosleep(0, 100000000);
    if (!$x) {
	return 0;
    }
    return 1/$x;
}

function a()
{
    time_nanosleep(0, 100000000);
    echo "Hello\n";
    newrelic_create_distributed_trace_payload();
};

a();
b();
