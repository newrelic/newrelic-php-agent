<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a temporary table must succeed with no error, under PHP's
default mysqli error reporting. The agent's explain runs on a second
connection, which cannot see the first connection's temporary tables.
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
1
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/nr_explain_tmp/select, 1
*/

/*EXPECT_TRACED_ERRORS
null
*/

/*EXPECT_ERROR_EVENTS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function test_prepare($link)
{
  $stmt = mysqli_prepare($link, "SELECT x FROM nr_explain_tmp WHERE x = 1");
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

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

mysqli_query($link, "CREATE TEMPORARY TABLE nr_explain_tmp (x INT)");
mysqli_query($link, "INSERT INTO nr_explain_tmp VALUES (1)");

test_prepare($link);
mysqli_close($link);
