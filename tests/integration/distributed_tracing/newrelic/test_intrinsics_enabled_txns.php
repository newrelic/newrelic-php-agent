<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the "always add" intrinsics are added when distributed tracing
is enabled.  This test tests the transaction event and transaction trace cases
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": "??"
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
        "traceId": "??",
        "sampled": "??",
        "priority": "??",
        "error": false
      },
      {},
      {}
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
          "??",
          "??",
          [
            "??",
            "??",
            "??",
            "??",
            [
              [
                "??",
                "??",
                "??",
                "??",
                [
                  [
                    "??",
                    "??",
                    "??",
                    "??",
                    "??"
                  ]
                ]
              ]
            ]
          ],
          {
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??",
              "guid": "??",
              "traceId": "??",
              "sampled": "??",
              "priority": "??"
            }
          }
        ],
        [
          "??",
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

/*EXPECT
Hello
*/

newrelic_add_custom_tracer('main');
function main()
{
  echo 'Hello';
}
main();

