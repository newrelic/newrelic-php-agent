<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A refused connection for the explain plan SHALL NOT break the application's
query.

To get an explain plan the agent opens a second connection with the
application's credentials. This test uses a MySQL user that is limited to one
connection, so the application's own connection takes the only slot and the
explain connection is refused.

On PHP 8.1 and later mysqli throws mysqli_sql_exception when a connection is
refused. If the agent did not catch it, the exception would be thrown into the
application's own query: "mysqli_sql_exception: User ... has exceeded the
'max_user_connections' resource". The TLS setup is in tests/tls (the user is
created by init.sql).
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'one_conn';
require("skipif_tls.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - query succeeded
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$link = new mysqli($MYSQL_TLS_HOST, 'nr_one_conn', 'nr_one_conn', $MYSQL_TLS_DB,
                   (int)$MYSQL_TLS_PORT);

try {
  $stmt = mysqli_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
  mysqli_stmt_execute($stmt);
  mysqli_stmt_bind_result($stmt, $value);
  while (mysqli_stmt_fetch($stmt)) {
  }
  tap_assert(true, 'query succeeded');
  mysqli_stmt_close($stmt);
} catch (Throwable $e) {
  echo "exception thrown into the application: " . get_class($e) . ": " . $e->getMessage() . "\n";
}

mysqli_close($link);
