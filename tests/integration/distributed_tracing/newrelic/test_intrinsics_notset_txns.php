<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that none of the "always add" intrinsics are added when distributed tracing
is disabled.  This test tests the transaction event and transaction trace cases
*/

/*INI
newrelic.transaction_tracer.threshold = 0
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
              "cpu_sys_time": "??"
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

