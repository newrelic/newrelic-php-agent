<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate explain plans when connections are made with
mysqli::init() and mysqli::real_connect().
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
STATISTICS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select",
      "scope":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},   [1, "??", "??", "??", "??", "??"]]
  ]
]*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = ?",
      "Datastore/statement/MySQL/TABLES/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": [
          [
            "id",
            "select_type",
            "table",
            "type",
            "possible_keys",
            "key",
            "key_len",
            "ref",
            "rows",
            "Extra"
          ],
          [
            [
              1,
              "SIMPLE",
              "TABLES",
              "ALL",
              null,
              "TABLE_NAME",
              null,
              null,
              null,
              "Using where; Skip_open_table; Scanned 1 database"
            ]
          ]
        ],
        "backtrace": [
          " in mysqli_stmt_execute called at __FILE__ (??)",
          " in test_prepare called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function test_prepare($link)
{
  $query = "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = 'STATISTICS'";

  $stmt = mysqli_prepare($link, $query);
  if (FALSE === $stmt) {
    echo mysqli_error($link) . "\n";
    return;
  }

  if (FALSE === mysqli_stmt_execute($stmt)) {
    echo mysqli_stmt_error($stmt) . "\n";
    return;
  }

  if (FALSE === mysqli_stmt_bind_result($stmt, $value)) {
    echo mysqli_stmt_error($stmt) . "\n";
    return;
  }

  while (mysqli_stmt_fetch($stmt)) {
    echo $value . "\n";
  }

  mysqli_stmt_close($stmt);
}

$link = mysqli_init();
$link->options(MYSQLI_OPT_CONNECT_TIMEOUT, 10);
$link->real_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_prepare($link);
mysqli_close($link);
