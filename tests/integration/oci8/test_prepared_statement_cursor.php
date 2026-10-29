<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A REF CURSOR that a PL/SQL block opens has no SQL that the agent can see, and
SHALL be reported as an operation, as before. The block itself is reported with
its SQL.
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
ok - block executed
ok - cursor executed
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Oracle/other, 2
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$conn = oci_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
tap_assert(false !== $conn, 'connected');

$cursor = oci_new_cursor($conn);
$block = oci_parse($conn, 'BEGIN OPEN :cursor FOR SELECT 1 AS one FROM dual; END;');
oci_bind_by_name($block, ':cursor', $cursor, -1, OCI_B_CURSOR);
tap_assert(oci_execute($block), 'block executed');

split_transaction();

tap_assert(oci_execute($cursor), 'cursor executed');
oci_free_statement($cursor);
oci_free_statement($block);
oci_close($conn);
