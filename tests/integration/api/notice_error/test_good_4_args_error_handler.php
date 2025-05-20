<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a traced error when newrelic_notice_error is
used as a callback handler for set_error_handler().
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "4 arg error",
      "NoticedError",
      {
        "stack_trace": [
          " in newrelic_notice_error called at ? (?)",
          " in trigger_error called at __FILE__ (??)",
          " in a called at __FILE__ (??)"
        ],
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
        "error.class": "NoticedError",
        "error.message": "4 arg error",
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

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "OtherTransaction\/php__FILE__",
      "guid": "??",
      "type": "Span",
      "category": "generic",
      "priority": "??",
      "sampled": true,
      "timestamp": "??",
      "nr.entryPoint": true,
      "transaction.name": "OtherTransaction\/php__FILE__"
    },
    {},
    {
    }
  ],
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "Custom\/a",
      "guid": "??",
      "type": "Span",
      "category": "generic",
      "priority": "??",
      "sampled": true,
      "timestamp": "??",
      "parentId": "??"
    },
    {},
    {
      "error.message": "4 arg error",
      "error.class": "NoticedError"
    }
  ]
]
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
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": true
      },
      {},
      {
        "errorType": "NoticedError",
        "errorMessage": "4 arg error"
      }
    ]
  ]
]
*/

function a() {
    trigger_error("4 arg error", E_USER_WARNING);
}

set_error_handler('newrelic_notice_error', E_USER_WARNING);
a();
