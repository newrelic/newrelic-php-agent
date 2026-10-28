<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A PDO statement of the odbc driver SHALL be reported the same way after the
transaction in which it was prepared has ended as in that transaction. The agent
does not parse the SQL of the odbc driver, so the statement is reported as an
operation with no slow SQL.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../skipif_odbc.inc');
*/

/*INI
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.transaction_tracer.explain_threshold = 0
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - statement prepared
ok - statement executed
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/ODBC/other, 1
*/

/*EXPECT_SLOW_SQLS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$pdo = new PDO("odbc:Driver={PostgreSQL Unicode};Server=$PG_HOST;Port=$PG_PORT;Database=postgres",
               $PG_USER, $PG_PW);
tap_assert($pdo instanceof PDO, 'connected');

$stmt = $pdo->prepare('SELECT usename FROM pg_user WHERE usename = ?');
tap_assert(false !== $stmt, 'statement prepared');

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

tap_assert($stmt->execute(array('postgres')), 'statement executed');
