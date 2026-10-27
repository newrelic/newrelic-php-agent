<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement with a placeholder, prepared in one transaction, SHALL be reported with its SQL when it is executed in the next transaction with its parameters passed to execute().
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80100) {
  die("skip: execute() takes parameters from PHP 8.1\n");
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
Datastore/statement/MySQL/tables/select, 1
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

$stmt = $link->prepare("SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?");
if (false === $stmt) {
  echo "prepare failed: " . $link->error . "\n";
  exit(1);
}

split_transaction();

$stmt->execute(['TRIGGERS']);
echo "executed\n";
$stmt->close();

