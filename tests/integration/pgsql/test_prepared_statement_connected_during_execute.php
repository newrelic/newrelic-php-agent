<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A pg_execute() whose parameter conversion runs code that connects again with the same connection string SHALL be reported against the instance of its connection.
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
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
in __toString
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[PG_HOST]/ENV[PG_PORT], 1
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

class Param {
  public function __toString(): string {
    global $PG_CONNECTION;
    echo "in __toString\n";
    pg_connect($PG_CONNECTION);
    return 'postgres';
  }
}

$conn = pg_connect($PG_CONNECTION);
if (false === pg_prepare($conn, 'nr_stmt', $users)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

pg_execute($conn, 'nr_stmt', array(new Param));
echo "executed\n";
