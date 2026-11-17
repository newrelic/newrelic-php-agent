<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Same as test_explain_init_command.php, but the connection is created with the
Pdo\Mysql::connect factory method. Each driver subclass has its own connect()
method record in the agent, so this exercises the record for Pdo\Mysql rather
than the one for PDO.

A query that reads a table created by the MYSQL_ATTR_INIT_COMMAND option
should still get an explain plan, which it does only if the agent saved the
options and replayed them on the connection it opens to run EXPLAIN.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
require(realpath (dirname ( __FILE__ )) . '/../../skipif_pdo_subclasses.inc');
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
ok - execute query on the init command table
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT * FROM nr_explain_tmp WHERE id = ?",
      "Datastore/statement/MySQL/nr_explain_tmp/select",
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

require_once(realpath(dirname(__FILE__)) . '/../../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');

function test_query($pdo)
{
  $stmt = $pdo->prepare('SELECT * FROM nr_explain_tmp WHERE id = 1');
  tap_assert($stmt->execute(), 'execute query on the init command table');
}

$pdo = Pdo\Mysql::connect($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, array(
  PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
  Pdo\Mysql::ATTR_INIT_COMMAND => 'CREATE TEMPORARY TABLE nr_explain_tmp (id INT)',
));

test_query($pdo);
