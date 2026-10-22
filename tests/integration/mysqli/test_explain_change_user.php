<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
After mysqli_change_user(), a slow query SHALL get an explain plan from the
database that the application switched to.

The agent opens its own connection to run EXPLAIN with the user, password and
database that it saved when the application connected. mysqli_change_user()
replaces all three on the application's connection. The test connects to
information_schema and then changes user to a table in the test database. The
query names the table without a database, so the explain connection can resolve
it only if the agent saved the new database. The TLS test server is used so that
the test has a database where it can create tables.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it. The table is created by tests/tls/init.sql.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'optional';
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

$link = mysqli_connect($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', 'information_schema', (int)$MYSQL_TLS_PORT);
tap_assert($link instanceof mysqli, 'connection');
tap_assert(mysqli_change_user($link, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB), 'change user');

$result = mysqli_query($link, 'SELECT * FROM nr_change_user_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);

