<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The database chosen with mysqli_select_db() SHALL be kept after the transaction
in which it was selected has ended. The test connects to information_schema and
selects the test database. The query in the second transaction names a table
without a database, so the explain connection can resolve it only if the agent
kept the selected database.

The table is created by tests/tls/init.sql. The test is skipped only if
MYSQL_TLS_HOST is not set or PHP is below 8.0, where the link data is not kept
across transactions; an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
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
ok - select db
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

$link = mysqli_connect($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional',
                       'information_schema', (int)$MYSQL_TLS_PORT);
tap_assert($link instanceof mysqli, 'connection');
tap_assert(mysqli_select_db($link, $MYSQL_TLS_DB), 'select db');

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

$result = mysqli_query($link, 'SELECT * FROM nr_change_user_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
