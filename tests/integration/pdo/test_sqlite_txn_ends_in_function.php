<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that a function of the application ends and starts again while
PDO::query() or PDOStatement::execute() runs the SQL that calls it SHALL NOT
break the agent, and the statements SHALL NOT be recorded in either
transaction.

The use of freed memory this guards against is only detected under valgrind.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/skipif_sqlite.inc');
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
*/

/*EXPECT
transaction restarted
transaction restarted
done
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/SQLite/select
*/

/*EXPECT_TRACED_ERRORS
null
*/

$pdo = new PDO('sqlite::memory:');
$pdo->sqliteCreateFunction('nr_end_txn', function () {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
  echo "transaction restarted\n";
  return 1;
});

$pdo->query('SELECT nr_end_txn()');

$stmt = $pdo->prepare('SELECT nr_end_txn()');
$stmt->execute();
echo "done\n";
