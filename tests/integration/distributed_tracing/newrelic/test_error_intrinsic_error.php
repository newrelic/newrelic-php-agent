<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Ensure the transaction event include the error intrinsic and that
the intrinsic's value is true for normal PHP errors.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": "??"
  },
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "error": true
      },
      {},
      {
        "errorType": "E_USER_ERROR",
        "errorMessage": "This is an error"
      }
    ]
  ]
]
*/

trigger_error("This is an error", E_USER_ERROR);
