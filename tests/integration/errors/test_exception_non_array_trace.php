<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record an exception whose trace property is not an array.
PHP before 8.0 allows this, and the agent must not use the value as a trace.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", ">=")) {
  die("skip: PHP >= 8.0 does not allow a non-array trace\n");
}
*/

/*INI
opcache.enable = 1
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Noticed exception 'Error' with message 'Sample Error' in __FILE__:??",
      "Error",
      {
        "stack_trace": [
          " in newrelic_notice_error called at __FILE__ (??)"
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
        "error.class": "Error",
        "error.message": "Noticed exception 'Error' with message 'Sample Error' in __FILE__:??",
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

$error = new Error('Sample Error');
$trace = new ReflectionProperty(Error::class, 'trace');
$trace->setAccessible(true);
$trace->setValue($error, 'not an array');

newrelic_notice_error($error);
