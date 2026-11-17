<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a temporary table, run with mysqli_query(), must succeed with no
error, under PHP's default mysqli error reporting. This is the mysqli_query()
form of test_explain_temporary_table.php, which uses a prepared statement. The
agent's explain runs on a second connection, which cannot see the first
connection's temporary tables.
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

function test_query($link)
{
  $result = mysqli_query($link, "SELECT x FROM nr_explain_tmp WHERE x = 1");
  if (FALSE === $result) {
    echo mysqli_error($link) . "\n";
    return;
  }

  while ($row = mysqli_fetch_row($result)) {
    echo $row[0] . "\n";
  }

  mysqli_free_result($result);
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

mysqli_query($link, "CREATE TEMPORARY TABLE nr_explain_tmp (x INT)");
mysqli_query($link, "INSERT INTO nr_explain_tmp VALUES (1)");

test_query($link);
mysqli_close($link);
