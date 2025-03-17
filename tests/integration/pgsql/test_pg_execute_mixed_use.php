<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for Postgres prepared statements.
*/

/*XFAIL tests that our agent currently doesn't handle mix-and-matching default vs. passed
 connections between pg_prepare and pg_execute */
 
/*SKIPIF
<?php
require("skipif.inc");
require("skipif_php81.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
pg_stats
pg_stats
pg_stats
pg_stats
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                                      [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                 [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/all"},                             [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/allOther"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/TABLES/select"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/TABLES/select",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Postgres/other"},                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Postgres/other",
      "scope":"OtherTransaction/php__FILE__"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Postgres/select"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},         [1, "??", "??", "??", "??", "??"]]
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
      "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = ?",
      "Datastore/statement/Postgres/TABLES/select",
      4,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in pg_execute called at __FILE__ (??)",
          " in my_pg_execute called at __FILE__ (??)",
          " in test_prepared_stmt called at __FILE__ (??)"
        ]
      }
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "(prepared statement)",
      "Datastore/operation/Postgres/other",
      3,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in pg_execute called at __FILE__ (??)",
          " in my_pg_execute called at __FILE__ (??)",
          " in test_prepared_stmt called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

function my_pg_prepare($conn, $name, $query)
{
    if (is_null($conn)) {
        return pg_prepare($name, $query);
    }
    return pg_prepare($conn, $name, $query);
}

function my_pg_execute($conn, $name, $query)
{
    if (is_null($conn)) {
        return pg_execute($name, $query);
    }
    return pg_execute($conn, $name, $query);
}

function my_pg_last_error($conn)
{
    if (is_null($conn)) {
        return pg_last_error();
    }
    return pg_last_error($conn);
}

function test_prepared_stmt($prepConn, $execConn, $name, $query)
{
    if (!my_pg_prepare($prepConn, $name, $query)) {
      echo my_pg_last_error($prepConn) . "\n";
      return;
    }

    $result = my_pg_execute($execConn, $name, array());
    if (!$result) {
        echo my_pg_last_error($execConn) . "\n";
        return;
    }

    while ($row = pg_fetch_row($result)) {
        echo implode(' ', $row) . "\n";
    }
}

$conn = pg_connect($PG_CONNECTION);
if (!$conn) {
  exit(1);
}

$query = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 'pg_stats'";

/* 1. prepare unnamed query with conn, execute without */
test_prepared_stmt($conn, null, '', $query);

/* 2. prepare named query with conn, execute without */
test_prepared_stmt($conn, null, 'prepare=with execute=without', $query);

/* 3. prepare named query without conn, execute with */
test_prepared_stmt(null, $conn, 'prepare=without execute=with', $query);

/* 4. prepare unnamed query without conn, execute with */
test_prepared_stmt(null, $conn, '', $query);

pg_close($conn);
