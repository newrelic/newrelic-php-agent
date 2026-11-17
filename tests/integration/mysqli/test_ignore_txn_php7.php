<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that transaction globals are properly freed when using New Relic API.
Before PHP 8.0 the data kept for the link is dropped when the transaction ends,
so the query in the second transaction gets no explain plan.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID >= 80000) {
  die("skip: see test_ignore_txn.php, the link data is kept across transactions from PHP 8.0\n");
}
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

newrelic_end_transaction(true);
newrelic_start_transaction(ini_get("newrelic.appname"));

test_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");

mysqli_close($link);
newrelic_end_transaction();
