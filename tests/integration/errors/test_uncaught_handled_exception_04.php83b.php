<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This test runs on PHPs 8.3.x only because of changes in PHP in the area of exception handling.
When a user exception handler unregisters itself as an exception handler when it handles uncaught exception,
the agent should record the error and add error attributes on all spans leading to uncaught exception as
well as the one throwing the exception. Error attributtes are not expected on the root span (because
the exception has been handled) as well as on the span created for exception handler.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled=1
newrelic.code_level_metrics.enabled = 0
display_errors=1
log_errors=0
*/


/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.3", "<")) {
  die("skip: PHP < 8.3.0 not supported\n");
}
*/


/*EXPECT_ERROR_EVENTS*/
/*
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
        "error.message": "Uncaught exception 'RuntimeException' with message 'Expected unexpected happened' in __FILE__:??",
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
      "??"
    ]
  ]
]
*/

/*EXPECT_SPAN_EVENTS*/
/*
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 4
  },
  [
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/call_throw_it",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Expected unexpected happened' in __FILE__:??",
        "error.class": "RuntimeException"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/throw_it",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Expected unexpected happened' in __FILE__:??",
        "error.class": "RuntimeException"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/user_exception_handler_02",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {}
    ]
  ]
]
*/


/*EXPECT_REGEX
01 Handled uncaught exception
*/

function user_exception_handler_01(Throwable $ex) {
  echo "01 Handled uncaught exception";
}

function user_exception_handler_02(Throwable $ex) {
  set_exception_handler('user_exception_handler_01');
  throw new RuntimeException("Not able to handle, throwing another exception from handler");
  echo "Should never see this";
}

function throw_it() {
  throw new RuntimeException('Expected unexpected happened');
}

function call_throw_it() {
  throw_it();
}

set_exception_handler('user_exception_handler_02');

call_throw_it();
