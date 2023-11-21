<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the newrelic.transaction_tracer.max_segments_cli is set to a non-zero value,
the agent limits the number of segments created, even in a nested scenario.
*/

/*INI
newrelic.transaction_tracer.max_segments_cli=10
newrelic.transaction_tracer.threshold=0
newrelic.distributed_tracing_enabled=0
newrelic.code_level_metrics.enabled=false
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "OtherTransaction/php__FILE__",
      "\u003cunknown\u003e",
      [
        [
          0, {}, {},
          [
            "?? start time", "?? end time", "ROOT", {},
            [
              [
                "?? start time", "?? end time", "`0", {},
                [
                  [
                    "?? start time", "?? end time", "`1", {},
                    [
                      [
                        "?? start time", "?? end time", "`2", {},
                        [
                          [
                            "?? start time", "?? end time", "`3", {}, []
                          ],
                          [
                            "?? start time", "?? end time", "`3", {}, []
                          ],
                          [
                            "?? start time", "?? end time", "`3", {}, []
                          ],
                          [
                            "?? start time", "?? end time", "`3", {}, []
                          ],
                          [
                            "?? start time", "?? end time", "`3", {}, []
                          ],
                          [
                            "?? start time", "?? end time", "`3", {}, []
                          ],
                          [
                            "?? start time", "?? end time", "`3", {}, []
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
          "OtherTransaction\/php__FILE__",
          "Custom\/great_grandmother",
          "Custom\/grandmother",
          "Custom\/my_function"
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

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/great_grandmother"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/great_grandmother",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/grandmother"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/grandmother",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function"},                                 [1000, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function",
      "scope":"OtherTransaction/php__FILE__"},                      [1000, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},               [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]]
  ]
]
*/




function my_function() {
    time_nanosleep(0, 50000); // force non-zero duration for the segment not to be dropped, 50usec should be enough
}
function grandmother(){
    for ($i = 0; $i < 1000; $i++) {
        my_function();
    }
}
function great_grandmother(){
    grandmother();
}

newrelic_add_custom_tracer("great_grandmother");
newrelic_add_custom_tracer("grandmother");
newrelic_add_custom_tracer("my_function");

great_grandmother();
