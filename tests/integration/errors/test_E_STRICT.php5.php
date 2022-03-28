<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report strict standards warnings.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", ">=")) {
  die("skip: PHP 7 not supported\n");
}
*/

/*INI
date.timezone = America/Los_Angeles
error_reporting = E_ALL | E_STRICT
*/

/*EXPECT_SCRUBBED

Strict Standards: mktime(): You should be using the time() function instead in __FILE__ on line ??
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "mktime(): You should be using the time() function instead",
      "Error",
      {
        "stack_trace": [
          " in mktime called at __FILE__ (??)"
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
        "error.message": "mktime(): You should be using the time() function instead",
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

/* Calling mktime() with no arguments causes E_STRICT. */
$current_time = mktime();
