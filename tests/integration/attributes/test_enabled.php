<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
With the default *.attributes.enabled settings, attributes should appear in
traced errors, transaction traces, and transaction events.
*/

/*INI
newrelic.transaction_tracer.threshold = 0
newrelic.special.expensive_node_min = 0
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_TRACED_ERRORS
[
  "??",
  [
    [
      "??",
      "OtherTransaction/php__FILE__",
      "Do you really want to report me?",
      "E_USER_ERROR",
      {
        "stack_trace": [
          " in trigger_error called at __FILE__ (??)"
        ],
        "userAttributes": {
          "hat": "who"
        },
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "error": true
      },
      {
        "hat": "who"
      },
      {
        "errorType": "E_USER_ERROR",
        "errorMessage": "Do you really want to report me?"
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
      "?? entry",
      "?? duration",
      "OtherTransaction/php__FILE__",
      "<unknown>",
      [
        [
          0, {}, {},
          "?? trace details",
          {
            "agentAttributes": {},
            "userAttributes": {
              "hat": "who"
            },
            "intrinsics": "??"
          }
        ],
        [
          "OtherTransaction/php__FILE__",
          "Custom/force_transaction_trace"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      "?? synthetics resource id"
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');

force_transaction_trace();

newrelic_add_custom_parameter("hat", "who");

trigger_error("Do you really want to report me?", E_USER_ERROR);
