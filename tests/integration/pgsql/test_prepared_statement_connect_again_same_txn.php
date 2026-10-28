<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared on a connection SHALL be reported with its SQL when pg_execute() runs it on the object that a later pg_connect() with the same connection string returned.
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
the second connect returned the same object
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

$users = "SELECT usename FROM pg_user WHERE usename = $1";

$first = pg_connect($PG_CONNECTION);
if (false === pg_prepare($first, 'nr_stmt', $users)) {
  echo "prepare failed: " . pg_last_error($first) . "\n";
  exit(1);
}

$second = pg_connect($PG_CONNECTION);
if ($second === $first) {
  echo "the second connect returned the same object\n";
}

pg_execute($second, 'nr_stmt', array('postgres'));
echo "executed\n";
