<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The SQL of a PDOStatement SHALL be known to the agent when the statement is
prepared in one transaction and executed in a later one. The agent reads the
SQL from the statement (PDOStatement::$queryString) and does not save it, so
ending the transaction does not lose it. The slow SQL trace of the second
transaction names the statement and the table.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_sqlite.inc');
*/

/*ENVIRONMENT
DATASTORE_PRODUCT=SQLite
DATASTORE_COLLECTION=sqlite_master
*/

/*INI
;comment=Set explain_threshold to 0 to ensure that the slow query is recorded.
newrelic.transaction_tracer.explain_threshold = 0
;comment=Disable instance and database name reporting (it is tested separately).
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
ok - prepared statement
ok - execute prepared statement in the next transaction
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select * from sqlite_master limit ?;",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": "??"
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

$conn = new PDO('sqlite::memory:');
$stmt = $conn->prepare('select * from sqlite_master limit 1;');
tap_assert(false !== $stmt, 'prepared statement');

newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

tap_assert($stmt->execute(), 'execute prepared statement in the next transaction');
