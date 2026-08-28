<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When PDO base class constructor is used to create connection object
to a database on a remote host via a TCP port,
and database instance reporting is enabled, the agent should
 - not generate errors
 - record datastore metrics
 - record a datastore instance metric
 - record a datastore span event with instance information
Moreover, when the query execution time exceeds the explain threshold,
the agent should record a slow sql trace with database instance information,
if fibers are involved.
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
DATASTORE_COLLECTION=test
MYSQL_PORT=3306
*/

/*INI
;comment=Set explain_threshold to 0 to ensure that the slow query is recorded.
newrelic.transaction_tracer.explain_threshold = 0
newrelic.fibers.disabled = false
*/

/*EXPECT
Starting Func 'a'
ok - create table
Ending Func 'a'
ok - drop table
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 5
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
        "name": "Custom\/test_instance_reporting",
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
        "parentId": "ENV[GUID_TEST_INSTANCE_REPORTING]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.hostname": "ENV[MYSQL_HOST]",
        "peer.address": "ENV[MYSQL_HOST]:ENV[MYSQL_PORT]",
        "db.instance": "ENV[MYSQL_DB]",
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
        "name": "Datastore\/statement\/MySQL\/test\/drop",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_INSTANCE_REPORTING]",
        "span.kind": "client",
        "component": "ENV[DATASTORE_PRODUCT]"
      },
      {},
      {
        "peer.hostname": "ENV[MYSQL_HOST]",
        "peer.address": "ENV[MYSQL_HOST]:ENV[MYSQL_PORT]",
        "db.instance": "ENV[MYSQL_DB]",
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
Datastore/all, 2
Datastore/allOther, 2
Datastore/instance/ENV[DATASTORE_PRODUCT]/ENV[MYSQL_HOST]/ENV[MYSQL_PORT], 2
Datastore/ENV[DATASTORE_PRODUCT]/all, 2
Datastore/ENV[DATASTORE_PRODUCT]/allOther, 2
Datastore/operation/ENV[DATASTORE_PRODUCT]/create, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/create, 1
Datastore/operation/ENV[DATASTORE_PRODUCT]/drop, 1
Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/drop, 1
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
      "DROP TABLE ENV[DATASTORE_COLLECTION];",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/drop",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          "/ in PDO::exec called at .*\/",
          " in test_instance_reporting called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)"
        ],
        "host": "ENV[MYSQL_HOST]",
        "port_path_or_id": "ENV[MYSQL_PORT]",
        "database_name": "ENV[MYSQL_DB]"
      }
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "CREATE TABLE ENV[DATASTORE_COLLECTION] (id INT, description VARCHAR(?));",
      "Datastore/statement/ENV[DATASTORE_PRODUCT]/ENV[DATASTORE_COLLECTION]/create",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          "/ in PDO::exec called at .*\/",
          " in test_instance_reporting called at ? (?)",
          " in Fiber::resume called at __FILE__ (??)"
        ],
        "host": "ENV[MYSQL_HOST]",
        "port_path_or_id": "ENV[MYSQL_PORT]",
        "database_name": "ENV[MYSQL_DB]"
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../test_instance_reporting.inc');

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
$fiber_pdo = new Fiber('test_instance_reporting');

$fiber_pdo->start(new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD), 0, true);
$fiber_a->start();
$fiber_pdo->resume();
$fiber_a->resume();
$fiber_pdo->resume();