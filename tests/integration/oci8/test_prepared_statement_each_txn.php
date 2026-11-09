<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An oci statement SHALL keep its SQL for every transaction it is executed in.
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
ok - executed in the first transaction
ok - executed in the second transaction
ok - executed in the third transaction
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Oracle/dual/select, 3
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$conn = oci_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $conn, 'connected');

$stmt = oci_parse($conn, 'SELECT 1 AS one FROM dual');

tap_assert(oci_execute($stmt), 'executed in the first transaction');
split_transaction();
tap_assert(oci_execute($stmt), 'executed in the second transaction');
split_transaction();
tap_assert(oci_execute($stmt), 'executed in the third transaction');

oci_free_statement($stmt);
oci_close($conn);
