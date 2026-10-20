<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Variant of test_E_DONT_BAIL_uncaught_exception.php: the uncaught Throwable is
a ParseError thrown by eval(). PHP 8 reports it as E_PARSE | E_DONT_BAIL,
not E_ERROR | E_DONT_BAIL.
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
^\s*(PHP )?Parse error:\s*syntax error, .* in .*\(\d+\) : eval\(\)'d code on line 1
HasDestructor::__destruct called\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'ParseError' with message 'syntax error, unexpected identifier \"is\"' in __FILE__(??) : eval()'d code:1",
      "ParseError",
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
        "error.class": "ParseError",
        "error.message": "Uncaught exception 'ParseError' with message 'syntax error, unexpected identifier \"is\"' in __FILE__(??) : eval()'d code:1",
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

$object = new HasDestructor();

eval('this is not valid php');
