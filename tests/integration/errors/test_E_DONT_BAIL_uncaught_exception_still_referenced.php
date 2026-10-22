<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent's exception handler must not release the uncaught exception more
often than it holds it. The script keeps two references to the exception and
a shutdown function drops one, so the exception must survive until the other
is destroyed at shutdown, as in stock PHP. One release too many destructs the
exception as soon as the shutdown function drops the first reference, while
the second still points to it.
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
^\s*(PHP )?Fatal error:\s*Uncaught KeptException: Thrown while still referenced in .*:\d+
Stack trace:
#0 {main}
  thrown in .* on line \d+
Shutdown function dropped one of two references
KeptException::__destruct called\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'KeptException' with message 'Thrown while still referenced' in __FILE__:??",
      "KeptException",
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
        "error.class": "KeptException",
        "error.message": "Uncaught exception 'KeptException' with message 'Thrown while still referenced' in __FILE__:??",
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

class KeptException extends RuntimeException {
  public function __destruct() {
    echo "KeptException::__destruct called\n";
  }
}

register_shutdown_function(function () {
  unset($GLOBALS['dropped']);
  echo "Shutdown function dropped one of two references\n";
});

$kept = $dropped = new KeptException('Thrown while still referenced');

throw $dropped;
