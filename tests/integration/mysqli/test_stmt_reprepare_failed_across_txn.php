<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement whose second prepare failed SHALL NOT be reported with the SQL of its first prepare when it is executed in a later transaction.
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
re-prepare failed
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/MySQL/other, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/MySQL/tables/select
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

$tables = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = 'TRIGGERS'";

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$stmt = $link->prepare($tables);
if (false === $stmt) {
  echo "prepare failed: " . $link->error . "\n";
  exit(1);
}

if (false === $stmt->prepare("SELECT x FROM nr_no_such_table")) {
  echo "re-prepare failed\n";
}

split_transaction();

$stmt->execute();
echo "executed\n";
$stmt->close();

