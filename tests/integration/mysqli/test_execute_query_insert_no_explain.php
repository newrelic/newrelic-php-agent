<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
An INSERT run with mysqli::execute_query() is reported as a slow SQL without an
explain plan: the agent explains only SELECT queries. The table is permanent,
since the explain runs on another connection, which cannot see a temporary
table. It is created before and dropped after the INSERT in transactions that
are ignored, so only the INSERT is reported.
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
inserted 1
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "INSERT INTO nr_insert_probe VALUES (?)",
      "Datastore/statement/MySQL/nr_insert_probe/insert",
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

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
$link->query('DROP TABLE IF EXISTS nr_insert_probe');
$link->query('CREATE TABLE nr_insert_probe (id INT)');
newrelic_end_transaction(true);
newrelic_start_transaction(ini_get("newrelic.appname"));

$link->execute_query('INSERT INTO nr_insert_probe VALUES (?)', [1]);
echo "inserted " . $link->affected_rows . "\n";

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));
$link->query('DROP TABLE nr_insert_probe');
newrelic_end_transaction(true);
$link->close();
