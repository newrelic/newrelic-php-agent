<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that an error handler ends and starts again while pg_query() is
failing SHALL NOT break the agent, and the query SHALL NOT be recorded in either
transaction.

The use of freed memory this guards against is only detected under valgrind.
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
query failed
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Postgres/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);

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

$result = pg_query($conn, 'SELECT * FROM nr_table_that_does_not_exist');
echo ($result === false) ? "query failed\n" : "query succeeded\n";

restore_error_handler();
