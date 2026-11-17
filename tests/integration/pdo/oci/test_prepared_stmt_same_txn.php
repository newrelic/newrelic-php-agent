<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A PDO statement of the oci driver SHALL be reported with its SQL when it is
executed in the transaction in which it was prepared.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../skipif_oci.inc');
*/

/*INI
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - statement prepared
ok - statement executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Oracle/dual/select, 1
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$pdo = new PDO('oci:dbname=//' . $ORACLE_CONNECT, $ORACLE_USER, $ORACLE_PASSWD);
tap_assert($pdo instanceof PDO, 'connected');

$stmt = $pdo->prepare('SELECT 1 AS one FROM dual');
tap_assert(false !== $stmt, 'statement prepared');

tap_assert($stmt->execute(), 'statement executed');
