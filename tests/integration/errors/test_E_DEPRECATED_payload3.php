<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that a PHP newrelic_create_distributed_trace_payload deprecation message is
shown.
*/

/*INI
error_reporting = E_ALL | E_STRICT
display_errors=1
log_errors=0
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_REGEX
^\s*(PHP )?Deprecated:\s*Function newrelic_create_distributed_trace_payload\(\)*
*/


/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction/php__FILE__",
      "Function newrelic_create_distributed_trace_payload() is deprecated.  Please see https://docs.newrelic.com/docs/agents/php-agent/features/distributed-tracing-php-agent#manual for more details.",
      "Error",
      {
        "stack_trace": [
          " in newrelic_create_distributed_trace_payload called at __FILE__ (??)"
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
        "error.message": "Function newrelic_create_distributed_trace_payload() is deprecated.  Please see https://docs.newrelic.com/docs/agents/php-agent/features/distributed-tracing-php-agent#manual for more details.",
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


$payload = newrelic_create_distributed_trace_payload();
