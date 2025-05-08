<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are disabled, test that calls to functions throwing exceptions that are not custom wrapped do not appear as spans in span events.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 50us
log_errors=0
display_errors=1
*/

/*EXPECT_REGEX
^\s*Fatal error: Uncaught Exception: I'm covered in bees!!!
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
        "error.message": "Uncaught exception 'Exception' with message 'I'm covered in bees!!!' in __FILE__:??",
        "error.class": "Exception"
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
      "Uncaught exception 'Exception' with message 'I'm covered in bees!!!' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
          " in custom_function_not_exceeding_tt_detail_threshold_but_throwing_exception called at __FILE__ (??)"
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
        "error.class": "Exception",
        "error.message": "Uncaught exception 'Exception' with message 'I'm covered in bees!!!' in __FILE__:??",
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

function custom_function_not_exceeding_tt_detail_threshold_but_throwing_exception() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  throw new Exception("I'm covered in bees!!!");
  echo 'custom_function_not_exceeding_tt_detail_threshold_but_throwing_exception called' . PHP_EOL;
}

custom_function_not_exceeding_tt_detail_threshold_but_throwing_exception();

echo 'Should not see this.'  . PHP_EOL;
