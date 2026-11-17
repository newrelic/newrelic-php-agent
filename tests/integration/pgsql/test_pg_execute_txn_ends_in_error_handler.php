<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that an error handler ends and starts again while pg_execute() is
failing SHALL NOT break the agent, and the statement SHALL NOT be recorded in
either transaction.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
transaction restarted
statement failed
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Postgres/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

// The use of freed memory that this test guards against is only detected
// under valgrind.
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);

pg_prepare($conn, 'nr_stmt', 'SELECT 1 / $1::int');

$restarted = false;
set_error_handler(function () use (&$restarted) {
  if (!$restarted) {
    $restarted = true;
    newrelic_end_transaction();
    newrelic_start_transaction(ini_get("newrelic.appname"));
    echo "transaction restarted\n";
  }
  return true;
});

/* Division by zero on the server: pg_execute() warns and returns false. */
$result = pg_execute($conn, 'nr_stmt', array(0));
echo ($result === false) ? "statement failed\n" : "statement succeeded\n";

restore_error_handler();
