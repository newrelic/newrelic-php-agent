<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a mysqli connection to a MySQL server that requires TLS SHALL get
an explain plan when the connection was opened with MYSQLI_CLIENT_SSL.

The agent opens its own connection to run EXPLAIN. The user for this test
requires TLS, so that connection needs TLS too. The agent replays the flags the
application passed to mysqli_real_connect(), including MYSQLI_CLIENT_SSL, and
the explain plan is produced. The TLS setup is in tests/tls.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'required';
require("skipif_tls.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - connection uses TLS
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
$connected = mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_required', 'tls_required',
                                 $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT, null, MYSQLI_CLIENT_SSL);
tap_assert($connected, 'connection uses TLS');

$stmt = mysqli_prepare($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'");
mysqli_stmt_execute($stmt);
mysqli_stmt_bind_result($stmt, $value);
while (mysqli_stmt_fetch($stmt)) {
}
tap_assert(true, 'query succeeded');

mysqli_stmt_close($stmt);
mysqli_close($link);
