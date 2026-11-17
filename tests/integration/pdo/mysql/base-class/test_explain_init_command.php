<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Control for test_explain_init_command_across_txn.php: when a PDO MySQL
connection is created with MYSQL_ATTR_INIT_COMMAND, the agent replays the
connection options on the connection it opens to run EXPLAIN, so a query that
reads a table created by the init command still gets an explain plan, as long
as the query runs in the same transaction as the connection.

The init command creates a temporary table. A temporary table exists only on
the connection that created it, so the explain connection can run EXPLAIN on a
query against it only if it ran the same init command.
*/

/*SKIPIF
<?php
require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
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

$pdo = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, array(
  PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
  PDO::MYSQL_ATTR_INIT_COMMAND => 'CREATE TEMPORARY TABLE nr_explain_tmp (id INT)',
));

test_query($pdo);
