<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report fatal errors.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", ">=")) {
  die("skip: PHP 7 not supported\n");
}
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Call to undefined function not_a_function()",
      "E_ERROR",
      {
        "stack_trace": [
          " in call_not_a_function called at __FILE__ (??)",
          " in call_not_a_function called at __FILE__ (??)"
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
        "error.class": "E_ERROR",
        "error.message": "Call to undefined function not_a_function()",
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

function call_not_a_function() {
  not_a_function();
}

call_not_a_function();
