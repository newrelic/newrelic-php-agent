<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report errors for exceptions that bubble to the top level and
are handled by a static method.
*/

/*EXPECT
In exception handler
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
        "error.class": "RuntimeException",
        "error.message": "Uncaught exception 'RuntimeException' with message 'Hi!' in __FILE__:??",
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

class Handler {
  public static function handleException($ex) {
    echo "In exception handler\n";
  }
}

set_exception_handler(array('Handler', 'handleException'));

function throw_it() {
  throw new RuntimeException('Hi!');
}

throw_it();
