<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should handle pg_query_params calls with bad params.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.stack_trace_threshold = 0
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = raw
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/operation/Postgres/other"},    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Postgres/other",
      "scope":"OtherTransaction/php__FILE__"},         [3, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                         [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/all"},                [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/allOther"},           [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "(unknown sql)",
      "Datastore/operation/Postgres/other",
      3,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in pg_query_params called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

pg_query_params();
pg_query_params(array());
pg_query_params(1, 1);
