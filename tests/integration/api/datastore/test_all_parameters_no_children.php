<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_record_datastore_segment() should make sure datastore segments don't 
have children.
*/

/*INI
newrelic.transaction_tracer.detail = 1
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MongoDB/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MongoDB/allOther"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/MongoDB/host.name/2222"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MongoDB/select"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MongoDB/table/select"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MongoDB/table/select",
      "scope":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_span_parameter"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/record_datastore_segment"},  [1, "??", "??", "??", "??", "??"]]
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
                      "host": "host.name",
                      "port_path_or_id": "2222",
                      "database_name": "db"
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
          "Datastore/statement/MongoDB/table/select"
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

function f() {
  /*
   * Creating a metric under normal circumstances ensures that the segment is
   * kept.
   */
  newrelic_add_custom_span_parameter("int", 7);
}

var_dump(newrelic_record_datastore_segment(function () {
  f();
  return 42;
}, array(
  'product'       => 'mongodb',
  'collection'    => 'table',
  'operation'     => 'select',
  'host'          => 'host.name',
  'portPathOrId'  => 2222,
  'databaseName'  => 'db',
  'query'         => 'SELECT * FROM table WHERE foo = 42',
)));
