<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a mysqli connection that was set up with mysqli_ssl_set() SHALL
get an explain plan when the MySQL user requires a client certificate.

The agent opens its own connection to run EXPLAIN. The user for this test
requires a client certificate, so that connection is refused unless it also
presents the key and certificate that the application gave to mysqli_ssl_set().
The agent saves the arguments of mysqli_ssl_set() and replays them on the
explain connection, so the explain plan is produced. Without that, the
connection is refused and, on PHP 8.1 and later, the error is thrown into the
application's query. The TLS setup is in tests/tls; the test is skipped without
it and on PHP below 7.4.
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
mysqli_ssl_set($link, "$MYSQL_TLS_CERTS/client-key.pem",
               "$MYSQL_TLS_CERTS/client-cert.pem", "$MYSQL_TLS_CERTS/ca.pem",
               null, null);
if (!@mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_clientcert', 'tls_clientcert',
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
ok - connection with a client certificate
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
mysqli_ssl_set($link, "$MYSQL_TLS_CERTS/client-key.pem",
               "$MYSQL_TLS_CERTS/client-cert.pem", "$MYSQL_TLS_CERTS/ca.pem",
               null, null);
$connected = mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_clientcert', 'tls_clientcert',
                                 $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT, null, MYSQLI_CLIENT_SSL);
tap_assert($connected, 'connection with a client certificate');

$stmt = mysqli_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
mysqli_stmt_execute($stmt);
mysqli_stmt_bind_result($stmt, $value);
while (mysqli_stmt_fetch($stmt)) {
}
tap_assert(true, 'query succeeded');

mysqli_stmt_close($stmt);
mysqli_close($link);
