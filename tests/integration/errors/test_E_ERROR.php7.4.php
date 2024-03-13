<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report fatal errors.
An E_ERROR is an uncaught exception and the expected error.class 
is recorded as Exception since it is recorded in the agent's uncaught 
exception logic and not via the normal error handling logic.
This is different from pre 7.4 which would report E_ERROR as the
error.class.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", "<")) {
  die("skip: PHP < 7.4.0 not supported\n");
}
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Uncaught exception 'Exception' with message '' in __FILE__:??",
      "Exception",
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
        "error.class": "Exception",
        "error.message": "Uncaught exception 'Exception' with message '' in __FILE__:??",
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

class C {
  public function __toString(): string {
    throw new Exception;
  }
}

(string) (new C);
