<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Statements that insert, update and delete, prepared in one transaction, SHALL be reported with their statement metrics when pg_execute() runs them in the next transaction.
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
Datastore/statement/Postgres/nr_stmt_tmp/insert, 1
Datastore/statement/Postgres/nr_stmt_tmp/update, 1
Datastore/statement/Postgres/nr_stmt_tmp/delete, 1
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

$conn = pg_connect($PG_CONNECTION);
pg_query($conn, "CREATE TEMPORARY TABLE nr_stmt_tmp (x INT)");

if (false === pg_prepare($conn, 'nr_insert', "INSERT INTO nr_stmt_tmp (x) VALUES (1)")
    || false === pg_prepare($conn, 'nr_update', "UPDATE nr_stmt_tmp SET x = 2 WHERE x = 1")
    || false === pg_prepare($conn, 'nr_delete', "DELETE FROM nr_stmt_tmp WHERE x = 2")) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

split_transaction();

pg_execute($conn, 'nr_insert', array());
pg_execute($conn, 'nr_update', array());
pg_execute($conn, 'nr_delete', array());
echo "executed\n";

