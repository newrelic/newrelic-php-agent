<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are disabled, test that calls to functions
that throw an exception which is caught by the caller do not appear as spans in
span events. Neither the function that threw nor the one that caught the
exception handed out a span ID or carries attributes, so they are dropped.
A segment is not kept just because an exception passed through it, and no
error is attached to the root span as the exception never reached the top.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 50us
log_errors=0
display_errors=1
*/

/*EXPECT
caught: I'm covered in bees!!!
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
  },
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
      {}
    ]
  ]
]
*/

/*EXPECT_ERROR_EVENTS
null
*/

function inner() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  throw new Exception("I'm covered in bees!!!");
}

function outer() {
  try {
    inner();
  } catch (Exception $e) {
    echo 'caught: ' . $e->getMessage() . PHP_EOL;
  }
}

outer();
