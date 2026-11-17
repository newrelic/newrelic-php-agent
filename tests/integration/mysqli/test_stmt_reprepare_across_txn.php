<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared a second time with different SQL SHALL be reported with the SQL of the second prepare when it is executed in a later transaction.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80000) {
  die("skip: the SQL of a statement is kept across transactions from PHP 8.0\n");
}
*/

/*INI
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/columns/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/MySQL/tables/select
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

$tables = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = 'TRIGGERS'";
$columns = "SELECT COLUMN_NAME FROM information_schema.columns WHERE column_name = 'TABLE_NAME'";

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$stmt = $link->stmt_init();
if (true !== $stmt->prepare($tables) || true !== $stmt->prepare($columns)) {
  echo "prepare failed: " . $stmt->error . "\n";
  exit(1);
}

split_transaction();

$stmt->execute();
echo "executed\n";
$stmt->close();

