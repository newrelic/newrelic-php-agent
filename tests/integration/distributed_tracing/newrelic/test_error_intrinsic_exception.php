<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Ensure the transaction event includes the error intrinsic and that
the intrinsic's value is true for Exceptions.
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
        "errorType": "Exception",
        "errorMessage": "Uncaught exception 'Exception' with message 'Hello Exception' in __FILE__:??"
      }
    ]
  ]
]
*/

throw new Exception("Hello Exception");
