<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a link whose options were set before the constructor connected
it SHALL get an explain plan.

A mysqli subclass initializes the object, sets an init command that creates a
temporary table, and then connects through the parent constructor. The
temporary table exists only on the connection that ran the command, so the
explain connection can read it only if the agent still has the option when the
constructor connects. A constructor on an object the agent has not seen
discards what is saved for that handle, because a new link can take the handle
of a freed one; an object it has seen keeps it.

The TLS test server is used so that the test has a database where it can create
tables; the test is skipped without it and on PHP below 8.0, where the agent
cannot tell a new link from one it has seen.
*/

/*SKIPIF
<?php
require("skipif.inc");
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
if (PHP_VERSION_ID < 80000) {
  die("skip: PHP >= 8.0 required\n");
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
      "SELECT * FROM nr_subclass_tmp WHERE id = ?",
      "Datastore/statement/MySQL/nr_subclass_tmp/select",
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

mysqli_report(MYSQLI_REPORT_OFF);

class Db extends mysqli
{
  public function __construct($host, $user, $password, $db, $port)
  {
    parent::init();
    $this->options(MYSQLI_INIT_COMMAND,
                   'CREATE TEMPORARY TABLE nr_subclass_tmp (id INT)');
    parent::__construct($host, $user, $password, $db, $port);
  }
}

$link = new Db($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB,
               (int)$MYSQL_TLS_PORT);
tap_assert(0 === mysqli_connect_errno(), 'connection');

$result = mysqli_query($link, 'SELECT * FROM nr_subclass_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
