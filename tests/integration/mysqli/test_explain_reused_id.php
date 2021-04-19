<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate explain plans even when object IDs are used.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
STATISTICS
STATISTICS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                           [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"}, [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
      "scope":"OtherTransaction/php__FILE__"},           [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [2, "??", "??", "??", "??", "??"]],
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
            "partitions",
            "type",
            "possible_keys",
            "key",
            "key_len",
            "ref",
            "rows",
            "filtered",
            "Extra"
          ],
          [
            [
              1,
              "SIMPLE",
              "tables",
              null,
              "ALL",
              null,
              "TABLE_NAME",
              null,
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
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=? AND ? = ?",
      "Datastore/statement/MySQL/tables/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": [
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

error_reporting(E_ALL);

// The bind_param warning is suppressed, so it is visible only when using a
// custom error handler.
set_error_handler(function ($errno, $errstr) {
  try {
    throw new Exception($errstr);
  } catch (Exception $e) {
    echo (string)$e;
  }

  return false;
});

function test_prepare($link, $query, $types = null, $params = null)
{
  $stmt = mysqli_prepare($link, $query);
  if (FALSE === $stmt) {
    echo mysqli_error($link) . "\n";
    return;
  }

  if ($types && $params) {
    $args = array($stmt, $types);
    foreach ($params as $param) {
      $args[] = &$param;
    }
    call_user_func_array('mysqli_stmt_bind_param', $args);
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

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS' AND ? = ?", 'ii', array(1, 1));
test_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
mysqli_close($link);
