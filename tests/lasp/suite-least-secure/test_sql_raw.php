<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the agent sends an obfuscated sql statement in transaction traces when
Language Agent Security Policy (LASP) configuration indicates record_sql:{enabled:true}
and agent is configured to send raw SQL. NOTE: RAW SQL is never sent for LASP enabled
accounts, irrespective of client configuration.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.record_sql = "raw"
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
                    "?? start time", "?? end time", "`1",
                    {
                      "host": "host.name",
                      "port_path_or_id": "2222",
                      "database_name": "db",
                      "sql_obfuscated": "SELECT * FROM table WHERE foo = ?"
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
