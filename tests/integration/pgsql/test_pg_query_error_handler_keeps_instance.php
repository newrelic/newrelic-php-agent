<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An error handler that does not close the connection SHALL leave the instance
of a failing pg_query() reported.
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
handler returned
query failed
*/

/*EXPECT_METRICS_EXIST
Datastore/Postgres/allOther, 1
Datastore/instance/Postgres/ENV[PG_HOST]/ENV[PG_PORT], 1
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$conn = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);

set_error_handler(function () {
  echo "handler returned\n";
  return true;
});

$result = pg_query($conn, 'SELECT * FROM nr_table_that_does_not_exist');
echo ($result === false) ? "query failed\n" : "query succeeded\n";

restore_error_handler();
