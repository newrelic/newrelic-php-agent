<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A PDO connection that takes the handle of a freed one SHALL use its own options
for the explain connection, not those of the connection that had the handle.

The first connection's init command creates one temporary table. After it is
freed, a second connection is built until it takes the same handle, with an
init command that creates a different temporary table. The query in
transaction 2 reads the second table, so it only gets an explain plan if the
explain connection ran the second connection's init command.

Transaction 1 is ignored (newrelic_end_transaction(true)) so only transaction 2
is checked.
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
ok - second connection reuses the first connection's handle
ok - execute query on the init command table
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT * FROM nr_second_tmp WHERE id = ?",
      "Datastore/statement/MySQL/nr_second_tmp/select",
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

function new_connection($table)
{
  global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

  return new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, array(
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::MYSQL_ATTR_INIT_COMMAND => "CREATE TEMPORARY TABLE $table (id INT)",
  ));
}

function test_query($pdo)
{
  $stmt = $pdo->prepare('SELECT * FROM nr_second_tmp WHERE id = 1');
  tap_assert($stmt->execute(), 'execute query on the init command table');
}

/* Transaction 1: the first connection, then connections until one takes its handle. */
$pdo = new_connection('nr_first_tmp');
$first_handle = spl_object_id($pdo);
unset($pdo);

$pdo = null;
$others = array();
for ($i = 0; $i < 50; $i++) {
  $pdo = new_connection('nr_second_tmp');
  if (spl_object_id($pdo) === $first_handle) {
    break;
  }
  /* Hold the ones that missed so the next one lands on another handle. */
  $others[] = $pdo;
  $pdo = null;
}
tap_equal($first_handle, spl_object_id($pdo),
          "second connection reuses the first connection's handle");

newrelic_end_transaction(true);
newrelic_start_transaction(ini_get("newrelic.appname"));

/* Transaction 2: query the second connection's temporary table. */
test_query($pdo);
