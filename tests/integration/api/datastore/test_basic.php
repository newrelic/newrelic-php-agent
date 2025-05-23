<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_record_datastore_segment() should record a datastore segment with the
minimum possible options.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/custom/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/custom/allOther"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/custom/unknown/unknown"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/custom/other"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/custom/other/other"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/custom/other/other",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/record_datastore_segment"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
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
          [
            "?? start time", "?? end time", "ROOT", "?? root attributes",
            [
              [
                "?? start time", "?? end time", "`0", "?? node attributes",
                [
                  [
                    "?? start time", "?? end time", "`1",
                    {
                      "host": "unknown",
                      "port_path_or_id": "unknown",
                      "database_name": "unknown"
                    },
                    []
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
          "OtherTransaction/php__FILE__",
          "Datastore/statement/custom/other/other"
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

/*EXPECT
int(42)
*/

var_dump(newrelic_record_datastore_segment(function () {
  /*
   *  Make sure this function takes at least 1 microsecond to ensure 
   * that a trace node is generated.
   */
  time_nanosleep(0, 1000);
  return 42;
}, array(
  'product' => 'custom',
)));
