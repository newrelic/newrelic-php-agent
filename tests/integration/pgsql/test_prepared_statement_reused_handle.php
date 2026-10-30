<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A connection that takes the object handle of a freed connection SHALL NOT find the statements prepared on the freed connection.
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
the third connection took the handle of the first
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Postgres/other, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/Postgres/pg_user/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$users = "SELECT usename FROM pg_user WHERE usename = $1";

$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
$first_id = spl_object_id($conn);
if (false === pg_prepare($conn, 'nr_stmt', $users)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

/*
 * The default link holds a reference to the most recent connection, so the
 * first connection is freed only when neither $conn nor the default link refers
 * to it: after the second connection replaces both.
 */
$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
if (spl_object_id($conn) === $first_id) {
  echo "the third connection took the handle of the first\n";
}

@pg_execute($conn, 'nr_stmt', array('postgres'));
echo "executed\n";
