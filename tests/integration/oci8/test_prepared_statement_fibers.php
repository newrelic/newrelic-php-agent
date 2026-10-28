<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An oci statement SHALL keep its SQL when it is parsed in one Fiber and executed
in another, in a later transaction.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: Fibers need PHP 8.1+\n");
}
*/

/*INI
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - statement executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Oracle/dual/select, 1
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$conn = oci_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $conn, 'connected');

$parse = new Fiber(function () use ($conn) {
  return oci_parse($conn, 'SELECT 1 AS one FROM dual');
});
$parse->start();
$stmt = $parse->getReturn();

split_transaction();

$execute = new Fiber(function () use ($stmt) {
  tap_assert(oci_execute($stmt), 'statement executed');
});
$execute->start();

oci_free_statement($stmt);
oci_close($conn);
