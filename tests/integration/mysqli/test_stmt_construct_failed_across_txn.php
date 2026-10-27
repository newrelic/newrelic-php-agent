<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement whose constructor failed to prepare it SHALL NOT be reported with the
SQL it was given.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80000) {
  die("skip: the SQL of a statement is kept across transactions from PHP 8.0\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
constructed
execute threw
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/MySQL/other, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/MySQL/nr_no_such_table/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/mysqli.inc');

mysqli_report(MYSQLI_REPORT_OFF);

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                       $MYSQL_PORT, $MYSQL_SOCKET);
if (false === $link) {
  echo "connect failed: " . mysqli_connect_error() . "\n";
  exit(1);
}

$stmt = new mysqli_stmt($link, "SELECT x FROM nr_no_such_table");
echo "constructed\n";

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

try {
  $stmt->execute();
  echo "execute did not throw\n";
} catch (Error $e) {
  echo "execute threw\n";
}
