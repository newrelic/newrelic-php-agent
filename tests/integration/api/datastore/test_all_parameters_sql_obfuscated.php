<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_record_datastore_segment() should record a datastore segment with all
possible options, while respecting query obfuscation.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.record_sql = "obfuscated"
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/MySQL/host.name/2222"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/table/select"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/table/select",
      "scope":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},        [1, "??", "??", "??", "??", "??"]],
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
                      "sql_obfuscated": "SELECT * FROM table WHERE foo = ?",
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
              "cpu_sys_time": "??"
            }
          }
        ],
        [
          "OtherTransaction/php__FILE__",
          "Datastore/statement/MySQL/table/select"
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
   * Make sure this function takes at least 1 microsecond to ensure that a trace
   * node is generated.
   */
  time_nanosleep(0, 1000);
  return 42;
}, array(
  'product'       => 'mysql',
  'collection'    => 'table',
  'operation'     => 'select',
  'host'          => 'host.name',
  'portPathOrId'  => 2222,
  'databaseName'  => 'db',
  'query'         => 'SELECT * FROM table WHERE foo = 42',
)));
