<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared in one transaction and executed in a later transaction of
the same PHP request SHALL be reported with its SQL: the datastore operation
and statement metrics, the slow SQL and the span db.statement.

The agent reads the SQL from the statement (PDOStatement::$queryString) and
does not save it, so ending the transaction does not lose it.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
*/

/*ENVIRONMENT
DATASTORE_PRODUCT=MySQL
DATASTORE_COLLECTION=tables
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
ok - execute prepared statement
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 1
Datastore/allOther, 1
Datastore/ENV[DATASTORE_PRODUCT]/all, 1
Datastore/ENV[DATASTORE_PRODUCT]/allOther, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/select, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select, 1
Supportability/TxnData/SlowSQL, 2
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/Custom/after_restart",
      "<unknown>",
      "?? SQL id",
      "select * from information_schema.tables limit ?;",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "explain_plan": "??",
        "backtrace": [
          "/ in PDOStatement::execute called at .*\/",
          " in exec_stmt called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "ENV[DATASTORE_PRODUCT]"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "select * from information_schema.tables limit ?;"
    }
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

function exec_stmt($stmt) {
  tap_assert($stmt->execute(), 'execute prepared statement');
}

$conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD);
$stmt = $conn->prepare('select * from information_schema.tables limit 1;');

// The prepare belongs to the first transaction; execute it in a second one.
newrelic_end_transaction();
newrelic_start_transaction(ini_get("newrelic.appname"));

exec_stmt($stmt);
newrelic_name_transaction("after_restart");
