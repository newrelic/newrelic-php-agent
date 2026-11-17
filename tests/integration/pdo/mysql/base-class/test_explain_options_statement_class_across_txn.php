<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Options that hold an object, such as the arguments of the statement class,
SHALL NOT keep the explain plan from working in a later transaction.

The agent keeps the scalar constructor options of a connection (here the init
command) for as long as the PDO object lives. An option that holds an object is
not kept, since it would have to be held past the end of the request's memory.
The explain connection does not need it.

The init command creates a temporary table, which exists only on the connection
that created it, so the explain plan in transaction 2 shows the init command
was kept. Transaction 1 is ignored (newrelic_end_transaction(true)) so only
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
class MyStatement extends PDOStatement
{
  protected function __construct($holder) {}
}

$pdo = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, array(
  PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
  PDO::MYSQL_ATTR_INIT_COMMAND => 'CREATE TEMPORARY TABLE nr_explain_tmp (id INT)',
  PDO::ATTR_STATEMENT_CLASS => array('MyStatement', array(new stdClass)),
));

newrelic_end_transaction(true);
newrelic_start_transaction(ini_get("newrelic.appname"));

/* Transaction 2: query the temporary table on the same connection. */
test_query($pdo);
