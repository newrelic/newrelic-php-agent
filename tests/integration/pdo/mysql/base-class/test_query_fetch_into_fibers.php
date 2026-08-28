<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record database metrics for the FETCH_INTO variant of
PDO::query() when PDO base class constructor is used to create connection
object, if fibers are involved.
*/

/*SKIPIF
<?php
require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP < 8.1.0 not supported\n");
}
*/

/*ENVIRONMENT
DATASTORE_PRODUCT=MySQL
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.fibers.disabled = false
*/

/*EXPECT
ok - create table
ok - insert row
Starting Func 'a'
ok - fetch row into object
Ending Func 'a'
ok - drop table
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 7
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
        "name": "Custom\/test_pdo_query",
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
        "name": "Datastore\/statement\/MySQL\/test\/create",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_PDO_QUERY]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "CREATE TABLE test (id INT, description VARCHAR(?));"
      }
    ],
    [
      {
        "category": "datastore",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Datastore\/statement\/MySQL\/test\/insert",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_PDO_QUERY]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "INSERT INTO test VALUES (?, ?);"
      }
    ],
    [
      {
        "category": "datastore",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Datastore\/statement\/MySQL\/test\/select",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_PDO_QUERY]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "SELECT * FROM test;"
      }
    ],
    [
      {
        "category": "datastore",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Datastore\/statement\/MySQL\/test\/drop",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_PDO_QUERY]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "DROP TABLE test;"
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
Datastore/all, 4
Datastore/allOther, 4
Datastore/ENV[DATASTORE_PRODUCT]/all, 4
Datastore/ENV[DATASTORE_PRODUCT]/allOther, 4
Datastore/operation/ENV[DATASTORE_PRODUCT]/create, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/test/create, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/insert, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/test/insert, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/select, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/test/select, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/drop, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/test/drop, 1
Supportability/PHP/Fiber/used
*/

require_once(realpath(dirname(__FILE__)) . '/../../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../test_query_fetch_into.inc');

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'a'\n";
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$fiber_a = new Fiber('a');
$fiber_pdo = new Fiber('test_pdo_query');

$fiber_pdo->start(new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD), 0, true);
$fiber_a->start();
$fiber_pdo->resume();
$fiber_a->resume();
$fiber_pdo->resume();
