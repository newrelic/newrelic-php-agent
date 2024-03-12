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
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP 5 not supported\n");
}
if (version_compare(PHP_VERSION, "7.4", ">=")) {
  die("skip: PHP >= 7.4.0 not supported\n");
}
*/

/*EXPECT_REGEX
^\s*(PHP )?(Catchable|Recoverable) fatal error:\s*Object of class stdClass could not be converted to string in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Object of class stdClass could not be converted to string",
      "E_RECOVERABLE_ERROR",
      {
        "stack_trace": [
          " in run_test_in_a_function called at __FILE__ (??)"
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
        "error.class": "E_RECOVERABLE_ERROR",
        "error.message": "Object of class stdClass could not be converted to string",
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

function run_test_in_a_function() {
  $cls = new stdClass();
  echo (string)$cls;
}

run_test_in_a_function();
