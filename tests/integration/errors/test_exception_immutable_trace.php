<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record an exception whose trace property holds an immutable
array. Frameworks set the property from a literal. Such an array has no
refcount and can be in memory that is not writable, for example opcache shared
memory with memory protection enabled, so the agent must not change it.
*/

/*INI
opcache.enable = 1
opcache.protect_memory = 1
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
          " in ? called at framework.php (??)"
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
if (PHP_VERSION_ID < 80100) {
  $trace->setAccessible(true);
}
$trace->setValue($error, [['file' => 'framework.php', 'line' => 1]]);

newrelic_notice_error($error);
