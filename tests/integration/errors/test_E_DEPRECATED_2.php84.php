<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report deprecation warnings.
PHP 8.4+ can no longer use E_USER_ERROR.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.4", "<")) {
  die("skip: PHP < 8.4.0 not supported\n");
}
*/

/*INI
error_reporting = E_ALL | E_STRICT
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*Deprecated: test\(\): Optional parameter \$a declared before required parameter \$b is implicitly treated as a required parameter in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "test(): Optional parameter $a declared before required parameter $b is implicitly treated as a required parameter",
      "E_DEPRECATED",
      {
        "stack_trace": "??",
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
        "error.class": "E_DEPRECATED",
        "error.message": "test(): Optional parameter $a declared before required parameter $b is implicitly treated as a required parameter",
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

function test($a = [], $b) {
  echo "Deprecated";
}
