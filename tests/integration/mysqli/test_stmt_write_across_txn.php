<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Statements that insert, update and delete, prepared in one transaction, SHALL be reported with their statement metrics when they are executed in the next transaction.
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
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/nr_stmt_tmp/insert, 1
Datastore/statement/MySQL/nr_stmt_tmp/update, 1
Datastore/statement/MySQL/nr_stmt_tmp/delete, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/MySQL/other
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

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$link->query("CREATE TEMPORARY TABLE nr_stmt_tmp (x INT)");

$insert = $link->prepare("INSERT INTO nr_stmt_tmp (x) VALUES (1)");
$update = $link->prepare("UPDATE nr_stmt_tmp SET x = 2 WHERE x = 1");
$delete = $link->prepare("DELETE FROM nr_stmt_tmp WHERE x = 2");
if (false === $insert || false === $update || false === $delete) {
  echo "prepare failed: " . $link->error . "\n";
  exit(1);
}

split_transaction();

$insert->execute();
$update->execute();
$delete->execute();
echo "executed\n";
$insert->close();
$update->close();
$delete->close();

