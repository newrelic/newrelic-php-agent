<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A pg_prepare() that fails on a new statement name SHALL NOT leave SQL for that name: pg_execute() of the name is reported with the operation other.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80100) {
  die("skip: the SQL of a statement is kept with the connection object from PHP 8.1\n");
}
*/

/*EXPECT
prepare failed
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Postgres/other, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/statement/Postgres/nr_no_such_table/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$conn = pg_connect($PG_CONNECTION);

if (false === @pg_prepare($conn, 'nr_bad', 'SELECT x FROM nr_no_such_table')) {
  echo "prepare failed\n";
}

split_transaction();

@pg_execute($conn, 'nr_bad', array());
echo "executed\n";
