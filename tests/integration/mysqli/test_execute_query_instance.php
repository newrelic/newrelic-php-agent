<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
mysqli::execute_query() and mysqli_execute_query() report the datastore
instance of the link they run on.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.2", "<")) {
  die("skip: mysqli_execute_query() exists from PHP 8.2\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = false
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
STATISTICS
STATISTICS
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/tables/select, 2
Datastore/instance/MySQL/ENV[MYSQL_HOST]/3306, 2
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$sql = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?";
$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
foreach ($link->execute_query($sql, ['STATISTICS']) as $row) {
  echo $row['TABLE_NAME'] . "\n";
}
foreach (mysqli_execute_query($link, $sql, ['STATISTICS']) as $row) {
  echo $row['TABLE_NAME'] . "\n";
}
$link->close();
