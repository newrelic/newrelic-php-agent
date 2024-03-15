<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report errors for exceptions that bubble to the top level even
when the exception handler stack is altered and a previous exception handler is
called.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT
In first exception handler
In second exception handler
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'RuntimeException' with message 'Hi!' in __FILE__:??",
      "RuntimeException",
      {
        "stack_trace": [
          " in throw_it called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": 
        {
          "totalTime": "??",
          "cpu_time": "??",
          "cpu_user_time": "??",
          "cpu_sys_time": "??",
          "guid": "??"
        }
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
        "error.message": "Uncaught exception 'RuntimeException' with message 'Hi!' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??"
      },
      {},
      {}
    ]
  ]
]
*/

function first($ex) {
  echo "In first exception handler\n";
}

function second($ex) {
  echo "In second exception handler\n";
}

set_exception_handler('first');
set_exception_handler('second');
set_exception_handler('first');
restore_exception_handler();

function throw_it() {
  throw new RuntimeException('Hi!');
}

first(null);
throw_it();
