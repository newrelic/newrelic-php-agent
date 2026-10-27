<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Statements that have the same name on two connections SHALL each be reported with the SQL prepared on their own connection when they are executed in the next transaction.
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
$roles = "SELECT rolname FROM pg_roles WHERE rolname = $1";

$first = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
$second = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
if (false === pg_prepare($first, 'nr_stmt', $users) || false === pg_prepare($second, 'nr_stmt', $roles)) {
  echo "prepare failed\n";
  exit(1);
}

split_transaction();

pg_execute($first, 'nr_stmt', array('postgres'));
pg_execute($second, 'nr_stmt', array('postgres'));
echo "executed\n";

