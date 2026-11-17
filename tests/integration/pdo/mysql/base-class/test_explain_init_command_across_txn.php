<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A query run in a later transaction on a PDO MySQL connection that was created
with MYSQL_ATTR_INIT_COMMAND should still get an explain plan.

The options passed to the PDO constructor are kept for as long as the PDO
object lives, not only for the transaction that created it. The connection the
agent opens to run EXPLAIN is built from the DSN, the username and the
password, which are read from the live connection, plus the saved options. If
the options were lost when the transaction ended, the explain connection would
be opened without the init command, the explain would fail silently and the
slow SQL would be recorded without an explain plan.

The init command creates a temporary table, which exists only on the
connection that created it. The explain connection can run EXPLAIN on a query
against that table only if it ran the same init command, so a missing explain
plan shows the options were lost. test_explain_init_command.php is the same
scenario inside one transaction.

Transaction 1 is ignored (newrelic_end_transaction(true)) so only
transaction 2 is checked.
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

/* Transaction 1: open the connection with the init command. */
$pdo = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, array(
  PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
  PDO::MYSQL_ATTR_INIT_COMMAND => 'CREATE TEMPORARY TABLE nr_explain_tmp (id INT)',
));

newrelic_end_transaction(true);
newrelic_start_transaction(ini_get("newrelic.appname"));

/* Transaction 2: query the temporary table on the same connection. */
test_query($pdo);
