<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a mysqli connection that takes its host, user and password from
the mysqli.default_* settings and is opened with MYSQLI_CLIENT_SSL SHALL get an
explain plan.

The application leaves out the host, the user and the password and only gives
the database, the port and the flags. The user for this test requires TLS, so
the connection the agent opens to run EXPLAIN is refused unless it also asks for
TLS. The flags are the seventh argument of mysqli_real_connect(), so the agent
can only pass them on if it also passes the three leading arguments. It fills
those in from the mysqli.default_* settings.

The arguments are null, which PHP replaces by the mysqli.default_* settings
from PHP 7.4. The TLS setup is in tests/tls; the test is skipped without it and
on PHP below 7.4.
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
;comment=Disable instance and database name reporting (it is tested separately).
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
mysqli.default_user = tls_required
mysqli.default_pw = tls_required
*/

/*EXPECT
ok - connection
ok - query succeeded
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT ? AS one",
      "Datastore/operation/MySQL/select",
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

/* The host is only known at run time. */
ini_set('mysqli.default_host', $MYSQL_TLS_HOST);

$link = mysqli_init();
$connected = mysqli_real_connect($link, null, null, null, $MYSQL_TLS_DB,
                                 (int)$MYSQL_TLS_PORT, null, MYSQLI_CLIENT_SSL);
tap_assert($connected, 'connection');

$result = mysqli_query($link, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
