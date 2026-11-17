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
tables. The test is skipped only if MYSQL_TLS_HOST is not set, on PHP below 8.0,
where the agent cannot tell a new link from one it has seen, and on PHP 8.5 and
above; an unusable server or user fails it.

PHP 8.5 and above throw "Cannot call constructor twice" when the constructor
runs on a link that init() or an earlier constructor call has initialized, so
the pattern does not exist there. The same options can be set before
real_connect() on those versions, and
test_explain_subclass_options_before_real_connect.php covers that.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
if (version_compare(PHP_VERSION, "8.5", ">=")) {
  die("skip: PHP >= 8.5.0 not supported\n");
}
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
