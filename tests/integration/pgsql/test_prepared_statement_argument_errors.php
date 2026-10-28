<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A pg_prepare() or pg_execute() that throws because of its arguments SHALL NOT change the statements prepared on the connection.
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
prepare threw
execute threw
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Postgres/pg_user/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/Postgres/pg_roles/select
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
if (false === pg_prepare($conn, 'nr_stmt', $users)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

try {
  pg_prepare($conn, 'nr_stmt');
  echo "prepare did not throw\n";
} catch (Throwable $e) {
  echo "prepare threw\n";
}

try {
  pg_execute($conn, 'nr_stmt', 'not an array');
  echo "execute did not throw\n";
} catch (Throwable $e) {
  echo "execute threw\n";
}

split_transaction();

pg_execute($conn, 'nr_stmt', array('postgres'));
echo "executed\n";
