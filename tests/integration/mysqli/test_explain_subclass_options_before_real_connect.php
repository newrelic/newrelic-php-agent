<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a link whose options were set before it was connected with
real_connect() SHALL get an explain plan.

A mysqli subclass initializes the object, sets an init command that creates a
temporary table, and then connects with real_connect(). The temporary table
exists only on the connection that ran the command, so the explain connection
can read it only if the agent still has the option when the link connects. This
is the form of the pattern that PHP 8.5 and above allow, because they throw when
the constructor is called twice.

The TLS test server is used so that the test has a database where it can create
tables. The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below
7.4; an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'optional';
require("skipif_tls.inc");
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
    $this->real_connect($host, $user, $password, $db, $port);
  }
}

$link = new Db($MYSQL_TLS_HOST, 'tls_optional', 'tls_optional', $MYSQL_TLS_DB,
               (int)$MYSQL_TLS_PORT);
tap_assert(0 === mysqli_connect_errno(), 'connection');

$result = mysqli_query($link, 'SELECT * FROM nr_subclass_tmp WHERE id = 1');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
