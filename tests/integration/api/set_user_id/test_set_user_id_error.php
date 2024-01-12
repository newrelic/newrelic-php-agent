<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_user_id() API:
 * Supportability/api/set_user_id metric is created
 * enduser.id agent attribute is present in error event and error trace for error events
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "??",
  "??",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_user_id"},                             [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},        [1, "??", "??", "??", "??", "??"]]
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
      "I'M COVERED IN BEES",
      "E_USER_ERROR",
      {
        "stack_trace": [
          " in trigger_error called at __FILE__ (??)",
          " in alpha called at __FILE__ (??)"
        ],
        "agentAttributes": {
          "enduser.id": "0123456789abcdefghijlkmnopqrstuvwxyz"
        },
        "intrinsics": {
          "totalTime": "??",
          "cpu_time": "??",
          "cpu_user_time": "??",
          "cpu_sys_time": "??",
          "guid": "??",
          "sampled": "??",
          "priority": "??",
          "traceId": "??"
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
    "reservoir_size": 100,
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "E_USER_ERROR",
        "error.message": "I'M COVERED IN BEES",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": "??",
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {
        "enduser.id": "0123456789abcdefghijlkmnopqrstuvwxyz"
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

function alpha() 
{
    trigger_error("I'M COVERED IN BEES", E_USER_ERROR);
};

$uuid = "0123456789abcdefghijlkmnopqrstuvwxyz";

$result = newrelic_set_user_id($uuid);

alpha();
