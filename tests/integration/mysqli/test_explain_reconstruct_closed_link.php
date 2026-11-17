<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
After a mysqli link is closed and its constructor is called again with other
credentials, a slow query SHALL get an explain plan with the new user and
password.

The first connection is made as nr_view_only, which may not read the table. The
link is closed and the constructor is called on the same object as tls_optional,
which may. The explain connection can read the table only if it uses the user
and password of the second connection and not those saved for the first. PHP 8.5
and above allow the constructor to run again only on a closed link, so the test
runs on every supported version. The TLS test server is used so that the test
has a database where it can create tables.
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
ok - first connection
ok - second connection
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

$link = new mysqli($MYSQL_TLS_HOST, 'nr_view_only', 'nr_view_only', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert(0 === $link->connect_errno, 'first connection');
$link->close();

$link->__construct($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert(0 === $link->connect_errno, 'second connection');

$result = mysqli_query($link, 'SELECT * FROM nr_change_user_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
