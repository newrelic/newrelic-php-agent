<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Variant of test_E_DONT_BAIL_uncaught_exception.php: the uncaught Throwable is
an Error that the engine throws inside a function.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Fatal error:\s*Uncaught Error: Call to undefined function no_such_function\(\) in .*:\d+
Stack trace:
#0 .*\(\d+\): call_missing_function\(\)
#1 {main}
  thrown in .* on line \d+
HasDestructor::__destruct called\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'Error' with message 'Call to undefined function no_such_function()' in __FILE__:??",
      "Error",
      {
        "stack_trace": [
          " in call_missing_function called at __FILE__ (??)"
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
        "error.class": "Error",
        "error.message": "Uncaught exception 'Error' with message 'Call to undefined function no_such_function()' in __FILE__:??",
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

class HasDestructor {
  public function __destruct() {
    echo "HasDestructor::__destruct called\n";
  }
}

function call_missing_function() {
  no_such_function();
}

$object = new HasDestructor();

call_missing_function();
