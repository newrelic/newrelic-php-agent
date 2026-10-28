<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli link SHALL keep the data the agent needs to run EXPLAIN after the
transaction in which it was opened has ended, when it is used from a Fiber. The
link is opened in the main code in the first transaction. A Fiber starts, is
suspended, and in the second transaction queries on the link.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(phpversion(), '8.1', '<')) {
  die("skip: PHP >= 8.1 required\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.fibers.disabled = false
;comment=Disable instance and database name reporting (it is tested separately).
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - connected
ok - query succeeded in the fiber
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT ? AS one",
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

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                   $MYSQL_PORT, $MYSQL_SOCKET);
tap_assert(0 === mysqli_connect_errno(), 'connected');

$fiber = new Fiber(function () use ($link) {
  Fiber::suspend();

  $result = mysqli_query($link, 'SELECT 1 AS one');
  tap_assert(false !== $result, 'query succeeded in the fiber');
  mysqli_free_result($result);
});

$fiber->start();

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

$fiber->resume();
mysqli_close($link);
