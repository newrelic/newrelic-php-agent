<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should capture and report fatal errors.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0.1", "<")) {
  die("skip: PHP < 7.0.1 not supported\n");
}
if (version_compare(PHP_VERSION, "7.4", ">=")) {
  die("skip: PHP >= 7.4.0 not supported\n");
}
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Method C::__toString() must not throw an exception, caught Exception: ",
      "E_ERROR",
      {
        "stack_trace": [
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
        "error.class": "E_ERROR",
        "error.message": "Method C::__toString() must not throw an exception, caught Exception: ",
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

class C {
  public function __toString(): string {
    throw new Exception;
  }
}

(string) (new C);
