<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A parse that fails SHALL NOT leave an entry for a later statement, and the
statements parsed after it SHALL be reported with their own SQL.
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
ok - parse on a closed connection did not give a statement
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

$closed = oci_new_connect($ORACLE_USER, $ORACLE_PASSWD, $ORACLE_CONNECT);
oci_close($closed);
try {
  $failed = @oci_parse($closed, 'SELECT COUNT(*) AS total FROM user_tables');
} catch (Throwable $e) {
  $failed = false;
}
tap_assert(!is_resource($failed), 'parse on a closed connection did not give a statement');

$stmt = oci_parse($conn, 'SELECT 1 AS one FROM dual');

split_transaction();

tap_assert(oci_execute($stmt), 'statement executed');
oci_free_statement($stmt);
oci_close($conn);
