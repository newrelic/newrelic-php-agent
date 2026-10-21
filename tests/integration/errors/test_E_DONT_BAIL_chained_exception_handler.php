<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Variant of test_E_DONT_BAIL_uncaught_exception_still_referenced.php: the
application's exception handler chains to the handler that
set_exception_handler() returned, newrelic_exception_handler, so the agent's
handler is called from PHP code. Symfony's ErrorHandler and the Sentry SDK
chain this way. The application's handler must continue after the call.
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
^\s*Previous exception handler: 'newrelic_exception_handler'
\s*(PHP )?Fatal error:\s*Uncaught ChainedException: Thrown with a chaining handler in .*:\d+
Stack trace:
#0 {main}
  thrown in .* on line \d+
Application exception handler continued
Shutdown function dropped one of two references
ChainedException::__destruct called\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'ChainedException' with message 'Thrown with a chaining handler' in __FILE__:??",
      "ChainedException",
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
        "error.class": "ChainedException",
        "error.message": "Uncaught exception 'ChainedException' with message 'Thrown with a chaining handler' in __FILE__:??",
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

class ChainedException extends RuntimeException {
  public function __destruct() {
    echo "ChainedException::__destruct called\n";
  }
}

register_shutdown_function(function () {
  unset($GLOBALS['dropped']);
  echo "Shutdown function dropped one of two references\n";
});

$previous = set_exception_handler(function (Throwable $exception) use (&$previous) {
  echo "Previous exception handler: ", var_export($previous, true), "\n";
  $previous($exception);
  echo "Application exception handler continued\n";
});

$kept = $dropped = new ChainedException('Thrown with a chaining handler');

throw $dropped;
