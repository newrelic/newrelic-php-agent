<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a traced error when newrelic_notice_error is
used as a callback handler for set_exception_handler().
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Noticed exception 'Exception' with message '1 arg exception' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
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
        "error.class": "Exception",
        "error.message": "Noticed exception 'Exception' with message '1 arg exception' in __FILE__:??",
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
      "error.message": "Noticed exception 'Exception' with message '1 arg exception' in __FILE__:??",
      "error.class": "Exception"
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
      "error.message": "Uncaught exception 'Exception' with message '1 arg exception' in __FILE__:??",
      "error.class": "Exception"
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
        "errorType": "Exception",
        "errorMessage": "Noticed exception 'Exception' with message '1 arg exception' in __FILE__:??"
      }
    ]
  ]
]
*/

function a()
{
  throw new Exception("1 arg exception");
}

set_exception_handler('newrelic_notice_error');
a();
