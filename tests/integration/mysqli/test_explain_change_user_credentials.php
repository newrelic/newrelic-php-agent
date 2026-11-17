<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
After mysqli_change_user(), a slow query SHALL get an explain plan with the
user and password that the application switched to.

The agent opens its own connection to run EXPLAIN with the user, password and
database that it saved when the application connected. mysqli_change_user()
replaces all three on the application's connection. The test connects as
nr_view_only, which may not read the table, and changes to tls_optional in the
same database, so the explain connection can read the table only if the agent
saved the new user and password. The TLS test server is used so that
the test has a database where it can create tables.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it. The table is created by tests/tls/init.sql.
*/

/*SKIPIF
<?php
require("skipif.inc");
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
ok - change user
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

$link = mysqli_connect($MYSQL_TLS_HOST, 'nr_view_only', 'nr_view_only', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert($link instanceof mysqli, 'connection');
tap_assert(mysqli_change_user($link, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB), 'change user');

$result = mysqli_query($link, 'SELECT * FROM nr_change_user_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);

