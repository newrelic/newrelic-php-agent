<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a mysqli connection that checks the server certificate against
the CA given to mysqli_ssl_set() SHALL get an explain plan.

The application gives a CA to mysqli_ssl_set() and turns on
MYSQLI_OPT_SSL_VERIFY_SERVER_CERT. The agent opens its own connection to run
EXPLAIN and replays the arguments of mysqli_ssl_set(), the options and the
flags on it. This test guards that the replay of the CA together with the
verification option does not break that connection: if it passed the arguments
in the wrong order or lost the option, the connection would fail and the
explain plan would be missing.

This is a control: it passes with and without the replay of mysqli_ssl_set(),
because without a CA the explain connection is not refused and mysqlnd does not
verify the server, which is not visible in the explain plan. It therefore does
not show that the CA reached the explain connection; the unit test of the saved
arguments covers that. The TLS setup is in tests/tls; the test is skipped
without it and on PHP below 7.4.
*/

/*SKIPIF
<?php
require("skipif.inc");
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
if (PHP_VERSION_ID < 70400) {
  die("skip: the TLS test users need PHP 7.4 or later\n");
}
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}
mysqli_report(MYSQLI_REPORT_OFF);
$link = mysqli_init();
mysqli_ssl_set($link, null, null, "$MYSQL_TLS_CERTS/ca.pem", null, null);
mysqli_options($link, MYSQLI_OPT_SSL_VERIFY_SERVER_CERT, 1);
if (!@mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_required', 'tls_required',
                          $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT, null, MYSQLI_CLIENT_SSL)) {
  die("skip: TLS MySQL server not available: " . mysqli_connect_error() . "\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - connection that checks the server certificate
ok - query succeeded
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?",
      "Datastore/statement/MySQL/tables/select",
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

$link = mysqli_init();
mysqli_ssl_set($link, null, null, "$MYSQL_TLS_CERTS/ca.pem", null, null);
mysqli_options($link, MYSQLI_OPT_SSL_VERIFY_SERVER_CERT, 1);
$connected = mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_required', 'tls_required',
                                 $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT, null, MYSQLI_CLIENT_SSL);
tap_assert($connected, 'connection that checks the server certificate');

$stmt = mysqli_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
mysqli_stmt_execute($stmt);
mysqli_stmt_bind_result($stmt, $value);
while (mysqli_stmt_fetch($stmt)) {
}
tap_assert(true, 'query succeeded');

mysqli_stmt_close($stmt);
mysqli_close($link);
