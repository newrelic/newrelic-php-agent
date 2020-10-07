<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report deprecation warnings.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, '5.5', '<') || version_compare(PHP_VERSION, '7.0', '>=')) {
  die("skip: requires PHP 5.5+ or HHVM\n");
}
*/

/*INI
error_reporting = E_ALL | E_STRICT
*/

/*EXPECT_SCRUBBED

Deprecated: preg_replace(): The /e modifier is deprecated, use preg_replace_callback instead in __FILE__ on line ??
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "preg_replace(): The /e modifier is deprecated, use preg_replace_callback instead",
      "Error",
      {
        "stack_trace": [
          " in preg_replace called at __FILE__ (??)"
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
        "error.message": "preg_replace(): The \/e modifier is deprecated, use preg_replace_callback instead",
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

preg_replace('(pattern)e', 'strtoupper("$1")', 'subject with pattern in it');
