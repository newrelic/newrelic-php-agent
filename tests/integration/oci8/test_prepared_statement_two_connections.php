<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Statements parsed on two connections SHALL each keep their own SQL when they
are executed in a later transaction.
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
ok - first connected
ok - second connected
ok - second executed
ok - first executed
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

$first_conn = oci_new_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $first_conn, 'first connected');
$second_conn = oci_new_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $second_conn, 'second connected');

$first = oci_parse($first_conn, 'SELECT 1 AS one FROM dual');
$second = oci_parse($second_conn, 'SELECT COUNT(*) AS total FROM user_tables');

split_transaction();

tap_assert(oci_execute($second), 'second executed');
tap_assert(oci_execute($first), 'first executed');
oci_free_statement($first);
oci_free_statement($second);
oci_close($first_conn);
oci_close($second_conn);
