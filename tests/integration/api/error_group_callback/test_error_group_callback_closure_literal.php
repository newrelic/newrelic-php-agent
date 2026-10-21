<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that newrelic_set_error_group_callback() keeps a closure literal alive
after the function that registered it returns. Only the agent refers to the
closure then. The closures created afterwards are the same size, so they would
take its memory if it were freed, and the agent would call one of them.
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 100,
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "Exception",
        "error.message": "Noticed exception 'Exception' with message 'Sample Exception' in __FILE__:??",
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
      {
        "error.group.name": "REGISTERED GROUP NAME"
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction\/php__FILE__",
      "Noticed exception 'Exception' with message 'Sample Exception' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
          " in alpha called at __FILE__ (??)"
        ],
        "agentAttributes": {
          "error.group.name": "REGISTERED GROUP NAME"
        },
        "intrinsics": {
          "totalTime": "??",
          "cpu_time": "??",
          "cpu_user_time": "??",
          "cpu_sys_time": "??",
          "guid": "??",
          "sampled": true,
          "priority": "??",
          "traceId": "??"
        }
      },
      "?? transaction ID"
    ]
  ]
]
*/

function register_error_group_callback()
{
  newrelic_set_error_group_callback(function ($txndata, $errdata) {
    return "REGISTERED GROUP NAME";
  });
}

function alpha()
{
  newrelic_notice_error(new Exception('Sample Exception'));
}

register_error_group_callback();

$others = array();
for ($i = 0; $i < 1000; $i++) {
  $others[] = function ($txndata, $errdata) {
    return "OTHER GROUP NAME";
  };
}

alpha();
