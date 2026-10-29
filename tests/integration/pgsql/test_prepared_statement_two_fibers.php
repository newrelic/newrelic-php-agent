<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Statements prepared in two Fibers on one connection SHALL each be reported with their own SQL when pg_execute() runs them in the other Fiber in the next transaction.
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
Datastore/statement/Postgres/pg_roles/select, 1
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

$roles = "SELECT rolname FROM pg_roles WHERE rolname = $1";

$first = new Fiber(function () use ($conn, $users) {
  pg_prepare($conn, 'nr_users', $users);
  Fiber::suspend();
  pg_execute($conn, 'nr_roles', array('postgres'));
});
$second = new Fiber(function () use ($conn, $roles) {
  pg_prepare($conn, 'nr_roles', $roles);
  Fiber::suspend();
  pg_execute($conn, 'nr_users', array('postgres'));
});
$first->start();
$second->start();

split_transaction();

$first->resume();
$second->resume();
echo "executed\n";
