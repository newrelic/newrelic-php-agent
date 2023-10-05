<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_user_id() API:
 * Supportability/api/set_user_id metric is created
 * enduser.id agent attribute is present in span event, txn trace, and analytics event
*/

/*INI
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT
ok - uuid set
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "??",
  "??",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},  [1, "??", "??", "??", "??", "??"]],
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


/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
  },
  [
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {
        "enduser.id": "0123456789abcdefghijlkmnopqrstuvwxyz"
      }
    ]
  ]
]
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "??",
      "??",
      "??",
      "??",
      [
        [
          "??",
          {},
          {},
          [
            "??",
            "??",
            "??",
            {},
            [
              [
                "??",
                "??",
                "??",
                {},
                []
              ]
            ]
          ],
          {
            "agentAttributes": {
              "enduser.id": "0123456789abcdefghijlkmnopqrstuvwxyz"
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
          }
        ],
        [
          "??"
        ]
      ],
      "??",
      "??",
      "??",
      "??",
      "??"
    ]
  ]
]
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 50,
    "events_seen": 1
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
        "error": false
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

$uuid = "0123456789abcdefghijlkmnopqrstuvwxyz";

$result = newrelic_set_user_id($uuid);

tap_assert($result, "uuid set");
