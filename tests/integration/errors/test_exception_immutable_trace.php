<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record an exception whose trace property holds an immutable
array. Frameworks set the property from a literal, and with opcache that array
lives in shared memory, so the agent must not change its refcount.
*/

/*INI
opcache.enable = 1
opcache.protect_memory = 1
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
$trace->setValue($error, [['file' => 'framework.php', 'line' => 1]]);

newrelic_notice_error($error);
