<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli_execute_query() that throws mysqli_sql_exception is reported as a
statement and gets no explain plan; the exception reaches the application.
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
ok - exception thrown
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/MySQL/nr_execute_query_missing/select, 1
Datastore/operation/MySQL/select, 1
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT * FROM nr_execute_query_missing WHERE id = ?",
      "Datastore/statement/MySQL/nr_execute_query_missing/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
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

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);

try {
  $link->execute_query("SELECT * FROM nr_execute_query_missing WHERE id = ?", [1]);
  echo "not ok - no exception\n";
} catch (mysqli_sql_exception $e) {
  echo "ok - exception thrown\n";
}
$link->close();
