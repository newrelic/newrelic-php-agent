<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Two oci statements parsed in one transaction SHALL each keep their own SQL when
they are executed in a later transaction.
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
ok - first executed
ok - second executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Oracle/dual/select, 1
Datastore/statement/Oracle/user_tables/select, 1
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$conn = oci_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $conn, 'connected');

$first = oci_parse($conn, 'SELECT 1 AS one FROM dual');
$second = oci_parse($conn, 'SELECT COUNT(*) AS total FROM user_tables');

split_transaction();

tap_assert(oci_execute($first), 'first executed');
tap_assert(oci_execute($second), 'second executed');
oci_free_statement($first);
oci_free_statement($second);
oci_close($conn);
