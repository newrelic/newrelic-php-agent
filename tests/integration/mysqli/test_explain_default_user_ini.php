<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a connection that takes its user from mysqli.default_user SHALL
get an explain plan.

The application gives the host and the password but leaves out the user, so
PHP uses mysqli.default_user. The arguments of the explain connection are
positional. If the agent only passed the arguments the application gave, the
password would take the place of the user and the login would be refused. The
agent fills in the missing leading arguments from the mysqli.default_* settings.

Before PHP 8.1 a null user is not replaced by mysqli.default_user, only an
omitted argument is, so there the application gives only the host and PHP takes
the user and password (and the port, set at run time) from the settings.
The TLS test server is used for the users; the test is skipped without it and
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
if (!@mysqli_real_connect($link, $MYSQL_TLS_HOST, 'tls_optional', 'tls_optional',
                          $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT)) {
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
mysqli.default_user = tls_optional
mysqli.default_pw = tls_optional
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

if (PHP_VERSION_ID < 80100) {
  ini_set('mysqli.default_port', (string)(int)$MYSQL_TLS_PORT);
  $link = mysqli_connect($MYSQL_TLS_HOST);
} else {
  $link = mysqli_connect($MYSQL_TLS_HOST, null, 'tls_optional', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
}
tap_assert($link instanceof mysqli, 'connection');

$result = mysqli_query($link, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
