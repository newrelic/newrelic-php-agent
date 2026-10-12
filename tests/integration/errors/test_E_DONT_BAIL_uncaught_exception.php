<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
PHP 8 reports an uncaught exception as E_ERROR | E_DONT_BAIL. E_DONT_BAIL
stops PHP's error handler from bailing out, so objects that are still alive
when the script ends get destructed at shutdown. The agent's error callback
must pass E_DONT_BAIL through to PHP's error handler while still recording
the exception. Verify that a global object's destructor runs after an
uncaught exception thrown at the top level.
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
^\s*(PHP )?Fatal error:\s*Uncaught RuntimeException: Thrown at the top level in .*:\d+
Stack trace:
#0 {main}
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
      "Uncaught exception 'RuntimeException' with message 'Thrown at the top level' in __FILE__:??",
      "RuntimeException",
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
        "error.class": "RuntimeException",
        "error.message": "Uncaught exception 'RuntimeException' with message 'Thrown at the top level' in __FILE__:??",
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

throw new RuntimeException('Thrown at the top level');
