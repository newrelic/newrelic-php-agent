<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Test that newrelic_end_transaction() ends all unended segments in the stack.
Additionally, unlike previously, we can gracefully close the segments with 
their proper names and parenting.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
newrelic.special.expensive_node_min = 0
newrelic.transaction_tracer.threshold = 0
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
                        "?? start time", "?? end time", "`2", "?? node attributes",
                        [ 
                          [
                            "?? start time", "?? end time", "`3", "?? node attributes",
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
              "cpu_sys_time": "??",
              "guid": "??",
              "sampled": true,
              "priority": "??",
              "traceId": "??"
            }
          }
        ],
        [
          "OtherTransaction\/php__FILE__",
          "Custom\/level_2",
          "Custom\/level_1",
          "Custom\/level_0"
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
