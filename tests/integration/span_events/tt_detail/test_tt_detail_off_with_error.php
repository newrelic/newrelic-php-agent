<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are disabled, test that calls to functions throwing errors that are not custom wrapped do not appear as spans in span events.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 50us
log_errors=0
display_errors=1
*/

/*EXPECT_REGEX
^\s*Fatal error: Uncaught Error: I'm covered in bees!!!
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
      {
        "error.message": "Uncaught exception 'Error' with message 'I'm covered in bees!!!' in __FILE__:??",
        "error.class": "Error"
      }
    ]
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
      "Uncaught exception 'Error' with message 'I'm covered in bees!!!' in __FILE__:??",
      "Error",
      {
        "stack_trace": [
          " in custom_function_not_exceeding_tt_detail_threshold_but_throwing_error called at __FILE__ (??)"
        ],
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
        "error.class": "Error",
        "error.message": "Uncaught exception 'Error' with message 'I'm covered in bees!!!' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
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

function custom_function_not_exceeding_tt_detail_threshold_but_throwing_error() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  throw new Error("I'm covered in bees!!!");
  echo 'custom_function_not_exceeding_tt_detail_threshold_but_throwing_error called' . PHP_EOL;
}

custom_function_not_exceeding_tt_detail_threshold_but_throwing_error();

echo 'Should not see this.'  . PHP_EOL;
