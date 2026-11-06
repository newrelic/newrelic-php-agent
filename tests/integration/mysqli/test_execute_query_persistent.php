<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
mysqli::execute_query() on a persistent connection is reported as a
statement and gets an explain plan.
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
STATISTICS
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/tables/select, 1
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?",
      "Datastore/statement/MySQL/tables/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": "??",
        "backtrace": "??"
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

$link = new mysqli('p:' . $MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

$result = $link->execute_query("SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?", ['STATISTICS']);
foreach ($result as $row) {
  echo $row['TABLE_NAME'] . "\n";
}
