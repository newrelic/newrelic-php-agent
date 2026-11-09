<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that a parameter's __toString() ends and starts again while
mysqli::execute_query() runs SHALL NOT break the agent, and the query SHALL NOT
be recorded in the new transaction.
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
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
transaction restarted
STATISTICS
done
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/MySQL/select
*/

// The use of freed memory that this test guards against is only detected
// under valgrind.
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

class EndsTransaction {
  public function __toString() {
    newrelic_end_transaction();
    newrelic_start_transaction(ini_get("newrelic.appname"));
    echo "transaction restarted\n";
    return "STATISTICS";
  }
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
$sql = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?";
foreach ($link->execute_query($sql, [new EndsTransaction()]) as $row) {
  echo $row['TABLE_NAME'] . "\n";
}
echo "done\n";
$link->close();
