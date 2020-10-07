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
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP 5 not supported\n");
}
if (version_compare(PHP_VERSION, "7.4", ">=")) {
  die("skip: PHP >= 7.4.0 not supported\n");
}
*/

/*INI
error_reporting = E_ALL | E_STRICT
*/

/*EXPECT_REGEX
^\s*(PHP )?Strict Standards:\s*htmlentities\(\):\s*Only basic entities substitution is supported for multi-byte encodings other than UTF-8; functionality is equivalent to htmlspecialchars in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "htmlentities(): Only basic entities substitution is supported for multi-byte encodings other than UTF-8; functionality is equivalent to htmlspecialchars",
      "Error",
      {
        "stack_trace": [
          " in htmlentities called at __FILE__ (??)"
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
        "error.message": "htmlentities(): Only basic entities substitution is supported for multi-byte encodings other than UTF-8; functionality is equivalent to htmlspecialchars",
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

/* Calling htmlentities() with a partially supported charset results in an
 * E_STRICT. */
htmlentities('', 0, 'Shift_JIS');
