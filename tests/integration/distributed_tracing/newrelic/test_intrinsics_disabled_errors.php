<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that none of the "always add" intrinsics are added when distributed tracing
is disabled.  This test tests the error data and error events cases.
*/

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.transaction_tracer.threshold=0
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction\/php__FILE__",
      "Uncaught exception 'Exception' with message 'An expected error to trigger error data and events' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
          " in main called at __FILE__ (??)"
        ],
        "agentAttributes": {},
        "intrinsics": {
          "totalTime": "??",
          "cpu_time": "??",
          "cpu_user_time": "??",
          "cpu_sys_time": "??",
          "guid": "??"
        }
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
    "events_seen": "??"
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "??",
        "error.message": "??",
        "transactionName": "??",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid" : "??"
      },
      {},
      {}
    ]
  ]
]
*/

newrelic_add_custom_tracer('main');
function main()
{
  throw new Exception("An expected error to trigger error data and events");
}
main();
