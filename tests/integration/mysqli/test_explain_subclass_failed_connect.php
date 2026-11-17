<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A failed call to mysqli_connect() made inside a method of a mysqli subclass
SHALL NOT replace what the agent saved for the object the method belongs to.

The agent opens its own connection to run EXPLAIN with what it saved when the
application connected. The failed call returns false and creates no link. If
the agent took the object that made the call for the new link, it would save the
failed call's server and user for it, and the explain connection of the later
slow query would be refused and the query would have no explain plan.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80000) {
  die("skip: PHP >= 8.0 required\n");
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
ok - connected
ok - the other connection failed
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

mysqli_report(MYSQLI_REPORT_OFF);

class Db extends mysqli
{
  /* Nothing listens on port 1: the call fails at once. */
  public function try_other()
  {
    return @mysqli_connect('127.0.0.1', 'nobody', 'nopass', 'nodb', 1);
  }
}

$link = new Db($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
               $MYSQL_PORT, $MYSQL_SOCKET);
tap_assert(0 === mysqli_connect_errno(), 'connected');

tap_assert(false === $link->try_other(), 'the other connection failed');

$result = mysqli_query($link, 'SELECT 1 AS one');
tap_assert(false !== $result, 'query succeeded');
mysqli_free_result($result);
mysqli_close($link);
