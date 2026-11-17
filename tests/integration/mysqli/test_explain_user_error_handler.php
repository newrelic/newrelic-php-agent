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
The server SHALL see the refused attempt, which shows that the agent tried to
open the explain connection.
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
ok - the agent tried to open the explain connection
ok - the handler was not called
ok - a later warning reaches the handler
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

// The server counts the connections it refuses. Another user reads the count
// before and after, so a refused explain connection shows as an increase. Other
// tests can refuse connections too, which can only add to the count.
$observer = new mysqli($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional',
                       $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);

function aborted_connects(mysqli $observer): int
{
    $row = $observer->query("SHOW GLOBAL STATUS LIKE 'Aborted_connects'")->fetch_row();
    return (int)$row[1];
}

$aborted_before = aborted_connects($observer);

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
tap_assert(aborted_connects($observer) > $aborted_before,
           'the agent tried to open the explain connection');
tap_equal(0, $calls, 'the handler was not called');

if (0 == $calls) {
  $handled = array();
  set_error_handler(function ($no, $str) use (&$handled) {
    $handled[] = $str;
    return true;
  });
  trigger_error('after explain', E_USER_WARNING);
  tap_equal(array('after explain'), $handled,
            'a later warning reaches the handler');
  mysqli_close($link);
}
mysqli_close($observer);
restore_error_handler();
