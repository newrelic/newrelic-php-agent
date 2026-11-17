<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli::execute_query() with an object among its parameters converts the
object once, and is reported as a slow SQL without an explain plan: the explain
would convert the object a second time and run its __toString() again.
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
conversions: 1
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
        "backtrace": "??"
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

class CountsConversions {
  public static $calls = 0;

  public function __toString() {
    self::$calls++;
    return "STATISTICS";
  }
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
$sql = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?";
foreach ($link->execute_query($sql, [new CountsConversions()]) as $row) {
  echo $row['TABLE_NAME'] . "\n";
}
echo "conversions: " . CountsConversions::$calls . "\n";
$link->close();
