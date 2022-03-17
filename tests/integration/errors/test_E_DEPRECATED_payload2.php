<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that a PHP newrelic_accept_distributed_trace_payload_httpsafe deprecation
message is shown.
*/

/*INI
error_reporting = E_ALL | E_STRICT
display_errors=1
log_errors=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Deprecated:\s*Function newrelic_accept_distributed_trace_payload_httpsafe\(\)*
*/


/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction/php__FILE__",
      "Function newrelic_accept_distributed_trace_payload_httpsafe() is deprecated.  Please see https://docs.newrelic.com/docs/agents/php-agent/features/distributed-tracing-php-agent#manual for more details.",
      "Error",
      {
        "stack_trace": [
          " in newrelic_accept_distributed_trace_payload_httpsafe called at __FILE__ (??)"
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
        "error.class": "Error",
        "error.message": "Function newrelic_accept_distributed_trace_payload_httpsafe() is deprecated.  Please see https://docs.newrelic.com/docs/agents/php-agent/features/distributed-tracing-php-agent#manual for more details.",
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


$payload = '{"v":[0,0],"d":{"ty":"App","ac":"111111","ap":"222222","id":"332c7b9a18777990","tr":"332c7b9a18777990","pr":1.28674,"sa":true,"ti":1530311294670}}';
newrelic_accept_distributed_trace_payload_httpsafe($payload);
