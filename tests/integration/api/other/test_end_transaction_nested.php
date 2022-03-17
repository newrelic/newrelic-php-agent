<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Test that newrelic_end_transaction() ends all unended segments in the stack.
*/

/*INI
newrelic.transaction_tracer.threshold = 0
newrelic.distributed_tracing_enabled=0
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
          [
            "?? start time", "?? end time", "ROOT", "?? root attributes",
            [
              [
                "?? start time", "?? end time", "`0", "?? node attributes",
                [
                  [
                    "?? start time", "?? end time", "`1", "?? node attributes",
                    [
                      [
                        "?? start time", "?? end time", "`1", "?? node attributes",
                        [ 
                          [
                            "?? start time", "?? end time", "`2", "?? node attributes",
                            []
                          ]
                        ]
                      ]
                    ]
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
          "OtherTransaction/php__FILE__",
          "<unknown>",
          "Custom/level_0"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      null
    ]
  ]
]
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

newrelic_add_custom_tracer("level_0");

function level_0() {
    echo "level_0\n";
}

function level_1() {
    level_0();
    newrelic_end_transaction();
}

function level_2() {
    level_1();
}

level_2();
