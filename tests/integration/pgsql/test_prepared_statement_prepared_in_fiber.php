<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared inside a Fiber in one transaction SHALL be reported with its SQL when pg_execute() runs it outside the Fiber in the next transaction.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80100) {
  die("skip: the SQL of a statement is kept with the connection object from PHP 8.1\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Postgres/pg_user/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Postgres/other
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$users = "SELECT usename FROM pg_user WHERE usename = $1";

$conn = pg_connect($PG_CONNECTION);

$fiber = new Fiber(function () use ($conn, $users) {
  pg_prepare($conn, 'nr_stmt', $users);
  Fiber::suspend();
});
$fiber->start();
$fiber->resume();

split_transaction();

pg_execute($conn, 'nr_stmt', array('postgres'));
echo "executed\n";
