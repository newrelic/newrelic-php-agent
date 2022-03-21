<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysqli_stmt_prepare.
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
      "scope":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},   [1, "??", "??", "??", "??", "??"]]
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
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
      "Datastore/statement/MySQL/tables/select",
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
              "tables",
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
          " in test_stmt_prepare called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

function test_stmt_prepare($link)
{
  $stmt = mysqli_stmt_init($link);
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'";
  if (FALSE === mysqli_stmt_prepare($stmt, $query) ||
      FALSE === mysqli_stmt_execute($stmt) ||
      FALSE === mysqli_stmt_bind_result($stmt, $name)) {
    echo mysqli_stmt_error() . "\n";
    mysqli_stmt_close($stmt);
    return;
  }

  while (mysqli_stmt_fetch($stmt)) {
    echo $name . "\n";
  }

  mysqli_stmt_close($stmt);
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_stmt_prepare($link);
mysqli_close($link);
