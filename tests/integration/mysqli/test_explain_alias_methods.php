<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a connection set up with mysqli::set_opt() and mysqli::connect()
SHALL get an explain plan, as it does with mysqli::options() and the
constructor.

mysqli::set_opt() is an alias of mysqli::options(), and mysqli::connect() is an
alias of mysqli_connect(). PHP gives each alias its own function entry, so the
agent needs its own instrumentation record for each one. The test sets an init
command that creates a temporary table, which exists only on the connection that
ran the command. The explain connection can read the table only if the agent
replays the option that the application set through the alias. The TLS test
server is used so that the test has a database where it can create tables; the
test is skipped without it and on PHP below 7.4.
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
*/

/*EXPECT
ok - option set
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
      "SELECT * FROM nr_alias_tmp WHERE id = ?",
      "Datastore/statement/MySQL/nr_alias_tmp/select",
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

$link = new mysqli();
tap_assert($link->set_opt(MYSQLI_INIT_COMMAND, 'CREATE TEMPORARY TABLE nr_alias_tmp (id INT)'), 'option set');
$connected = $link->connect($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);
tap_assert($connected, 'connection');

$result = $link->query('SELECT * FROM nr_alias_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
$result->free();
$link->close();
