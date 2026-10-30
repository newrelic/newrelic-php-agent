<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An error handler that closes the connection while pg_query() is failing SHALL
NOT break the query's segment, and pg_query() SHALL still be reported, without
the instance of the connection that was closed.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
handler closed the connection
query failed
*/

/*EXPECT_METRICS_EXIST
Datastore/Postgres/allOther, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/instance/Postgres/ENV[PG_HOST]/ENV[PG_PORT]
*/

/*EXPECT_TRACED_ERRORS
null
*/

// The use of freed memory that this test guards against is only detected
// under valgrind.
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);

set_error_handler(function () use (&$conn) {
  pg_close($conn);
  echo "handler closed the connection\n";
  return true;
});

$result = pg_query($conn, 'SELECT * FROM nr_table_that_does_not_exist');
echo ($result === false) ? "query failed\n" : "query succeeded\n";

restore_error_handler();
