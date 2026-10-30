<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A connection SHALL report statements it has many of with their SQL when they were prepared or executed recently, and a statement that was prepared long ago and not used since SHALL be reported with the operation other once more statements than the connection keeps have been prepared after it.
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
Datastore/statement/Postgres/pg_user/select, 9
Datastore/statement/Postgres/pg_database/select, 1
Datastore/operation/Postgres/other, 2
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

$roles = "SELECT rolname FROM pg_roles WHERE rolname = $1";
$databases = "SELECT datname FROM pg_database WHERE datname = $1";

/* Prepared first and not used again until the end. */
if (false === pg_prepare($conn, 'nr_first', $roles)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}
if (false === pg_prepare($conn, 'nr_hot', $users)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

/* More names than a connection keeps; the hot statement is used throughout. */
$fillers = 2000;
for ($i = 0; $i < $fillers; $i++) {
  if (false === pg_prepare($conn, "nr_f$i", $databases)) {
    echo "prepare failed: " . pg_last_error($conn) . "\n";
    exit(1);
  }
  if (0 == $i % 250) {
    pg_execute($conn, 'nr_hot', array('postgres'));
  }
}

split_transaction();

/* Dropped: prepared first and not used since. */
@pg_execute($conn, 'nr_first', array('postgres'));
/* Dropped: one of the oldest names that were prepared. */
@pg_execute($conn, 'nr_f10', array('postgres'));
/* Kept: used throughout. */
pg_execute($conn, 'nr_hot', array('postgres'));
/* Kept: the newest. */
pg_execute($conn, 'nr_f' . ($fillers - 1), array('postgres'));
echo "executed\n";
