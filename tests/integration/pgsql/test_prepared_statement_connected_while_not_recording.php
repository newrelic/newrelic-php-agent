<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared with pg_prepare() and run with pg_execute(), both with no connection argument, on a connection that was opened while no transaction was running, SHALL be reported with its SQL.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*EXPECT
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

newrelic_end_transaction();
$conn = pg_connect($PG_CONNECTION);
newrelic_start_transaction(ini_get("newrelic.appname"));

if (false === @pg_prepare('nr_stmt', $users)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

@pg_execute('nr_stmt', array('postgres'));
echo "executed\n";
