<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When a second constructor call on a connected mysqli link throws, a slow query
on the link SHALL get an explain plan with the user and password of the first
connection.

From PHP 8.5, calling the constructor on a link that is not closed throws
"Cannot call constructor twice". The link is connected as tls_optional, which
may read the table, and the second call names nr_view_only, which may not. The
explain connection can read the table only if the agent kept the credentials of
the connection that is still open and did not save those of the call that
threw. The TLS test server is used so that the test has a database where it can
create tables.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 8.5;
an unusable server or user fails it. The table is created by tests/tls/init.sql.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.5", "<")) {
  die("skip: PHP < 8.5.0 not supported\n");
}
$tls_profile = 'view_only';
require("skipif_tls.inc");
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
ok - connection
ok - second constructor threw
ok - query succeeded
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT * FROM nr_change_user_tmp WHERE id = ?",
      "Datastore/statement/MySQL/nr_change_user_tmp/select",
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

$link = new mysqli($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert(0 === $link->connect_errno, 'connection');

try {
  $link->__construct($MYSQL_TLS_HOST, 'nr_view_only', 'nr_view_only', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
  tap_assert(false, 'second constructor threw');
} catch (Error $e) {
  tap_assert('Cannot call constructor twice' === $e->getMessage(), 'second constructor threw');
}

$result = mysqli_query($link, 'SELECT * FROM nr_change_user_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
