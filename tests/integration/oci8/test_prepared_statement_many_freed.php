<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Statements that were freed SHALL NOT affect the SQL of the one that is kept,
when many statements are parsed and freed in between.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - executed
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

$kept = oci_parse($conn, 'SELECT 1 AS one FROM dual');

for ($i = 0; $i < 500; $i++) {
  $other = oci_parse($conn, "SELECT $i AS n FROM user_tables");
  oci_free_statement($other);
  unset($other);
}

split_transaction();

tap_assert(oci_execute($kept), 'executed');
oci_free_statement($kept);
oci_close($conn);
