<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the newrelic.transaction_tracer.max_segments_cli is set to a non-zero value,
the agent limits the number of segments created, including datastore segments.
The agent must still create metrics for functions for which add_custom_tracer
was called.
*/

/*INI
newrelic.transaction_tracer.max_segments_cli=3
newrelic.transaction_tracer.threshold=0
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
      "\u003cunknown\u003e",
      [
        [
          0, {}, {},
          [
            "?? start time", "?? end time", "ROOT", {}, [
              [
                "?? start time", "?? end time", "`0", {}, [
                  [
                    "?? start time", "?? end time", "`1", 
                    {}, []
                  ],
                  [
                    "?? start time", "?? end time", "`2",
		                {
                      "host": "host.name",
                      "database_name": "db",
                      "port_path_or_id": "2222",
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
              "cpu_sys_time": "??"
            }
          }
        ],
        [
          "OtherTransaction\/php__FILE__",
          "Custom\/my_function_3",
          "Datastore\/statement\/MySQL\/table\/select"
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
    [{"name":"Custom/my_function_1"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function_2"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function_3"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/MySQL/host.name/2222"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/table/select"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},               [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/record_datastore_segment"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function_1",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function_2",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/my_function_3",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/table/select",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*
 * We add multiple functions of differing durations to ensure deterministic dropping of certain segments.
 * Because multiple segments of the same parent are dropped by a swap-n-pop removal from their parent's
 * children list, we need to be able to determistically know which of these 3 functions is shortest
 */
function my_function_1(){
    usleep(10); // force non-zero duration for the segment not to be dropped; duration needs to be shorter than datastore segment
}
function my_function_2(){
    usleep(2000); // force non-zero duration for the segment not to be dropped; duration needs to be shorter than datastore segment
}
function my_function_3(){
    usleep(4000); // force non-zero duration for the segment not to be dropped; duration needs to be shorter than datastore segment
}

newrelic_add_custom_tracer("my_function_1");
newrelic_add_custom_tracer("my_function_2");
newrelic_add_custom_tracer("my_function_3");

my_function_3();
my_function_2();
my_function_1();

newrelic_record_datastore_segment(function () {
    usleep(6000); return 42; // force non-zero duration for the segment not to be dropped; duration needs to be longer than user func segment
}, array(
    'product'       => 'mysql',
    'collection'    => 'table',
    'operation'     => 'select',
    'host'          => 'host.name',
    'portPathOrId'  => 2222,
    'databaseName'  => 'db',
    'query'         => 'SELECT * FROM table WHERE foo = 42',
));
