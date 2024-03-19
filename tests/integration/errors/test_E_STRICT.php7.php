<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report strict standards warnings.
PHP 7 makes E_STRICT irrelevant, reclassifying most of the errors as proper warnings, 
notices or E_DEPRECATED: https://wiki.php.net/rfc/reclassify_e_strict
The E_STRICT constant will be retained for better compatibility, it will simply no longer have meaning in PHP 7.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: PHP 5 not supported\n");
}
if (version_compare(PHP_VERSION, "7.4", ">=")) {
  die("skip: PHP 7.4+ not supported\n");
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
      "E_STRICT",
      {
        "stack_trace": [
          " in htmlentities called at __FILE__ (??)"
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
        "error.class": "E_STRICT",
        "error.message": "htmlentities(): Only basic entities substitution is supported for multi-byte encodings other than UTF-8; functionality is equivalent to htmlspecialchars",
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

/* Calling htmlentities() with a partially supported charset results in an
 * E_STRICT. */
htmlentities('', 0, 'Shift_JIS');
