<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report runtime warnings along with a stack trace.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", ">=")) {
  die("skip: PHP 7 not supported\n");
}
*/

/*EXPECT_SCRUBBED
Warning: Division by zero in __FILE__ on line ??
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Division by zero",
      "E_WARNING",
      {
        "stack_trace": [
          " in run_test called at __FILE__ (??)",
          " in run_test called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
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
        "error.class": "E_WARNING",
        "error.message": "Division by zero",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??"
      },
      {},
      {}
    ]
  ]
]
*/

function run_test() {
  echo 8 / 0;
}

run_test();
