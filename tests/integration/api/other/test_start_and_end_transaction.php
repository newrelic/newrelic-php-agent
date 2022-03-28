<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Start and end a transaction while segments are created.
 */

/*INI
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 1
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
          "Custom/f2"
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


newrelic_add_custom_tracer("f1");
newrelic_add_custom_tracer("f2");
newrelic_add_custom_tracer("main");


function f1() {
    echo "f1\n";
}

function f2() {
    echo "f2\n";
}

function main() {
    f1();
    newrelic_ignore_transaction();
    newrelic_end_transaction();
    newrelic_start_transaction(ini_get("newrelic.appname"));
    f2();
}

main();
