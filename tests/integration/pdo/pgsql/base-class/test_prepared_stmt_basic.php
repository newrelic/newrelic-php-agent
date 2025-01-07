<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When PDO base class constructor is used to create connection object
and a query is executed via PDOStatement::execute(), the agent should
 - not generate errors
 - record datastore metrics
 - record a datastore span event
Moreover, when the query execution time exceeds the explain threshold,
the agent should record a slow sql trace without explain plan.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_pgsql.inc');
*/

/*ENVIRONMENT
DATASTORE_PRODUCT=Postgres
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
Supportability/TxnData/SlowSQL, 1
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select * from information_schema.tables limit ?;",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          "/ in PDOStatement::execute called at .*\/",
          " in test_prepared_stmt called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

// peer.address is unknown:unknown because instance_reporting is disabled
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

require_once(realpath (dirname ( __FILE__ )) . '/../../test_prepared_stmt_basic.inc');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

$query = 'select * from information_schema.tables limit 1;';
test_prepared_stmt(new PDO($PDO_PGSQL_DSN, $PG_USER, $PG_PW), $query);
