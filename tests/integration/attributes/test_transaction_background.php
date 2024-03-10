<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
In a background transactions, the agent should NOT include web transaction
attributes in error traces, error events, and analytic events.
*/

/*HEADERS
X-Request-Start=1368811467146000
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "I'M COVERED IN BEES!",
      "NoticedError",
      {
        "stack_trace": [
          " in newrelic_notice_error called at __FILE__ (??)"
        ],
        "agentAttributes": {
          "request.uri": "__FILE__",
          "SERVER_NAME": "127.0.0.1",
          "request.headers.host": "127.0.0.1"
        },
        "intrinsics": "??",
        "request_uri": "__FILE__"
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
        "error.class": "NoticedError",
        "error.message": "I'M COVERED IN BEES!",
        "transactionName": "OtherTransaction/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {
        "request.uri": "__FILE__",
        "SERVER_NAME": "127.0.0.1",
        "request.headers.host": "127.0.0.1"
      }
    ]
  ]
]
*/

/*EXPECT_ANALYTICS_EVENTS
 [
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "error": true,
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??"
      },
      {
      },
      {
        "errorType": "NoticedError",
        "errorMessage": "I'M COVERED IN BEES!",
        "request.uri": "__FILE__",
        "request.headers.host": "127.0.0.1"
      }
    ]
  ]
]
*/

header('Content-Length: 898');
header('Content-Type: text/plain');
newrelic_background_job (true);
newrelic_notice_error("I'M COVERED IN BEES!");
