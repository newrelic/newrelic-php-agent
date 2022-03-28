<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report recoverable errors.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", ">=")) {
  die("skip: PHP 7 not supported\n");
}
*/

/*EXPECT_SCRUBBED
Catchable fatal error: Object of class stdClass could not be converted to string in __FILE__ on line ??
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Object of class stdClass could not be converted to string",
      "Error",
      {
        "stack_trace": [
          " in run_test_in_a_function called at __FILE__ (??)",
          " in run_test_in_a_function called at __FILE__ (??)"
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
        "error.class": "Error",
        "error.message": "Object of class stdClass could not be converted to string",
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

function run_test_in_a_function() {
  $cls = new stdClass();
  echo (string)$cls;
}

run_test_in_a_function();
