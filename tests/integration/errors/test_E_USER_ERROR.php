<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report user-generated fatal errors along with a
stack trace.
*/

/*INI
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Fatal error:\s*Sample E_USER_ERROR in .*? on line [0-9]+\s*$
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Sample E_USER_ERROR",
      "E_USER_ERROR",
      {
        "stack_trace": [
          " in trigger_error called at __FILE__ (??)"
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
        "error.class": "E_USER_ERROR",
        "error.message": "Sample E_USER_ERROR",
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

trigger_error("Sample E_USER_ERROR", E_USER_ERROR);
