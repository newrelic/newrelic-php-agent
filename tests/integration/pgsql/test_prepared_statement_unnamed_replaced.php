<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An unnamed statement prepared a second time SHALL be reported with the SQL of the second prepare when pg_execute() runs it in the same transaction.
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
Datastore/statement/Postgres/pg_roles/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/Postgres/pg_user/select
Datastore/operation/Postgres/other
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$users = "SELECT usename FROM pg_user WHERE usename = $1";

$roles = "SELECT rolname FROM pg_roles WHERE rolname = $1";

$conn = pg_connect($PG_CONNECTION);
if (false === pg_prepare($conn, '', $users) || false === pg_prepare($conn, '', $roles)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

pg_execute($conn, '', array('postgres'));
echo "executed\n";
