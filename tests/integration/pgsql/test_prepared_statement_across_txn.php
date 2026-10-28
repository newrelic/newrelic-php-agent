<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A statement prepared with pg_prepare() in one transaction SHALL be reported with its SQL when pg_execute() runs it in the next transaction: the statement metric, the slow SQL and the span db.statement.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 80100) {
  die("skip: the SQL of a statement is kept with the connection object from PHP 8.1\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.record_sql = obfuscated
;comment=Set explain_threshold to 0 to ensure that the slow query is recorded.
newrelic.transaction_tracer.explain_threshold = 0
*/

/*EXPECT
executed
*/

/*EXPECT_METRICS_EXIST
Datastore/statement/Postgres/pg_user/select, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Postgres/other
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT usename FROM pg_user WHERE usename = $?",
      "Datastore/statement/Postgres/pg_user/select",
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

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore/statement/Postgres/pg_user/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "Postgres"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "SELECT usename FROM pg_user WHERE usename = $?"
    }
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function split_transaction() {
  newrelic_end_transaction();
  newrelic_start_transaction(ini_get("newrelic.appname"));
}

$users = "SELECT usename FROM pg_user WHERE usename = $1";
$roles = "SELECT rolname FROM pg_roles WHERE rolname = $1";

$conn = pg_connect($PG_CONNECTION);
if (false === pg_prepare($conn, 'nr_stmt', $users)) {
  echo "prepare failed: " . pg_last_error($conn) . "\n";
  exit(1);
}

split_transaction();

pg_execute($conn, 'nr_stmt', array('postgres'));
echo "executed\n";

