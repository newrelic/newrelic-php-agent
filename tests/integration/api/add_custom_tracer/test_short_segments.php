<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that traces will be generated even for very short segments.
 */

/*INI
newrelic.transaction_tracer.threshold=0
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
          0, {}, {}, [
            "?? start time", "?? end time", "ROOT", "?? root attributes", [
              [
                "?? start time", "?? end time", "`0", "?? node attributes", "?? node children" 
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
          "Custom/my_function"
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function"},                                   [1000, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function",
      "scope":"OtherTransaction/php__FILE__"},                        [1000, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



function my_function() {
    printf('');
}

newrelic_add_custom_tracer("my_function");

/*
 * We call this function 1000 times, as the PHP engine needs some time to warm
 * up and to create very short segment. When just calling the function one
 * time, the segment duration never is below 1ms.
 */
for ($i = 0; $i < 1000; $i++) {
    my_function();
}
