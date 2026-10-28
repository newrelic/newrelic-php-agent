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
flags on it. The test checks that this connection is opened with the CA and the
verification option, and that the explain plan is produced. The TLS setup is in
tests/tls.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'verify_ca';
require("skipif_tls.inc");
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
