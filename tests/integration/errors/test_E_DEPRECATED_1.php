<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report deprecation warnings. For PHP 5.3 and 5.4,
we use the split() function to trigger the warning.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, '5.3', '<') || version_compare(PHP_VERSION, '5.5', '>=')) {
  die("skip: requires PHP 5.3 or 5.4\n");
}
*/

/*INI
error_reporting = E_ALL | E_STRICT
*/

/*EXPECT_SCRUBBED

Deprecated: Function split() is deprecated in __FILE__ on line ??
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction/php__FILE__",
      "Function split() is deprecated",
      "Error",
      {
        "stack_trace": [
          " in split called at __FILE__ (??)"
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
        "error.message": "Function split() is deprecated",
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

list($month, $day, $year) = split('[/.-]', '01/01/1970');
