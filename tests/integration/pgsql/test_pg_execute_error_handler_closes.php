<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An error handler that closes the connection while pg_execute() is failing SHALL NOT break the execute's segment, and pg_execute() SHALL still be reported with the SQL of its statement.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
handler closed the connection
execute failed
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

$sql = "SELECT usename FROM pg_user WHERE usename = $1 AND 1 / $2::int = 1";

$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
if (false === pg_prepare($conn, 'nr_stmt', $sql)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

set_error_handler(function () use (&$conn) {
  pg_close($conn);
  echo "handler closed the connection\n";
  return true;
});

/* The division by zero is an error on the server: pg_execute() warns and returns false. */
$result = pg_execute($conn, 'nr_stmt', array('postgres', '0'));
echo (false === $result) ? "execute failed\n" : "execute succeeded\n";

restore_error_handler();
