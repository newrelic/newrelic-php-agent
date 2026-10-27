<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The application's error handler SHALL NOT be called by the connection that the
agent opens to run EXPLAIN, and so cannot close the application's link while
the agent is using the data saved for it.

The test uses a MySQL user that is limited to one connection, so the explain
connection is refused. With mysqli not throwing, the refusal is a warning,
which calls the handler of the application. The handler here closes the link.
The TLS setup is in tests/tls (the user is created by init.sql).
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
ok - the handler was not called
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

$link = new mysqli($MYSQL_TLS_HOST, 'nr_one_conn', 'nr_one_conn', $MYSQL_TLS_DB,
                   (int)$MYSQL_TLS_PORT);

$calls = 0;
set_error_handler(function () use (&$calls, &$link) {
  $calls++;
  mysqli_close($link);
  return true;
});

$stmt = mysqli_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
$ok = $stmt && mysqli_stmt_execute($stmt);
if ($ok) {
  mysqli_stmt_bind_result($stmt, $value);
  while (mysqli_stmt_fetch($stmt)) {
  }
  mysqli_stmt_close($stmt);
}
tap_assert($ok, 'query succeeded');
tap_equal(0, $calls, 'the handler was not called');

restore_error_handler();
if (0 == $calls) {
  mysqli_close($link);
}
