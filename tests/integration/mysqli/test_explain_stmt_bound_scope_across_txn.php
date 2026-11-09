<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli statement whose parameters were bound to variables of a function that has since returned SHALL be explained when it is executed in a later transaction.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80000) {
  die("skip: the link and SQL of a statement are kept across transactions from PHP 8.0\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
;comment=Disable instance and database name reporting (it is tested separately).
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - statement prepared
ok - parameters bound
ok - statement executed
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT ? AS a, ? AS b",
      "Datastore/operation/MySQL/select",
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

// EXPLAIN output does not depend on the values of the parameters, so this test
// checks that a plan was produced. The letters that stand for the types of the
// parameters have a unit test.
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
tap_assert(0 === mysqli_connect_errno(), 'connected');

$stmt = mysqli_prepare($link, 'SELECT ? AS a, ? AS b');
tap_assert(false !== $stmt, 'statement prepared');

function bind_in_function($stmt) {
  $number = 7;
  $text = 'seven';
  return mysqli_stmt_bind_param($stmt, 'is', $number, $text);
}

tap_assert(bind_in_function($stmt), 'parameters bound');

split_transaction();

tap_assert(mysqli_stmt_execute($stmt), 'statement executed');
mysqli_stmt_close($stmt);
mysqli_close($link);
