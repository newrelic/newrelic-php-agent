<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When PDO base class constructor is used to create connection object
and a query is executed via PDOStatement::execute() with value bound,
the agent should
- not generate errors
- record datastore metrics
- record a datastore span event
Moreover, when the query execution time exceeds the explain threshold,
the agent should record a slow sql trace with explain plan, if fibers are involved.
*/

/*SKIPIF
<?php
require(realpath (dirname ( __FILE__ )) . '/../../skipif_pgsql.inc');
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP < 8.1.0 not supported\n");
}
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
newrelic.fibers.disabled = false
*/

/*EXPECT
Starting Func 'a'
Ending Func 'a'
ok - execute prepared statement
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 4
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "ENV[GUID_ROOT]",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/test_prepared_stmt",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_ROOT]"
      },
      {},
      {}
    ],
    [
      {
        "category": "datastore",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Datastore\/statement\/ENV[DATASTORE_PRODUCT]\/ENV[DATASTORE_COLLECTION]\/select",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_PREPARED_STMT]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "select * from information_schema.tables where table_name = ? limit ?;"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/a",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_ROOT]"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 1
Datastore/allOther, 1
Datastore/ENV[DATASTORE_PRODUCT]/all, 1
Datastore/ENV[DATASTORE_PRODUCT]/allOther, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/select, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select, 1
Supportability/TxnData/SlowSQL, 1
Supportability/PHP/Fiber/used
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select * from information_schema.tables where table_name = ? limit ?;",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          "/ in PDOStatement::execute called at .*\/",
          " in test_prepared_stmt called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../test_prepared_stmt_bind_value.inc');

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'a'\n";
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$query = 'select * from information_schema.tables where table_name = ? limit 1;';
$fiber_a = new Fiber('a');
$fiber_pdo = new Fiber('test_prepared_stmt');

$fiber_pdo->start(new PDO($PDO_PGSQL_DSN, $PG_USER, $PG_PW), $query, true);
$fiber_a->start();
$fiber_pdo->resume();
$fiber_a->resume();
$fiber_pdo->resume();
