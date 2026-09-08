<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysqli::query if fibers are involved.
Note, this is not true async.
Because mysqli->query() is synchronous, the entire PHP script blocks on that line until the database responds.
To achieve true, non-blocking asynchronous execution where the Fiber pauses while the database is processing, 
you must use MySQL's asynchronous flag (MYSQLI_ASYNC) along with reap_async_query().
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT
ok - test_query1 (query)
ok - test_query1 (fetch)
Starting Func 'a'
ok - test_query2 (query)
ok - test_query2 (fetch)
Ending Func 'a'
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
        "name": "Custom\/test_mysqli",
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
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/test_query1",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_MYSQLI]"
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
        "name": "Datastore\/statement\/MySQL\/tables\/select",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_MYSQLI_Q1]",
        "span.kind": "client",
        "component": "MySQL"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/test_query2",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_MYSQLI]"
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
        "name": "Datastore\/statement\/MySQL\/tables\/select",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "ENV[GUID_TEST_MYSQLI_Q2]",
        "span.kind": "client",
        "component": "MySQL"
      },
      {},
      {
        "peer.address": "unknown:unknown",
        "db.statement": "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?"
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

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"},              [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                  [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                             [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/get_linking_metadata"},              ["??", "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/Fiber/used"},                        ["??", "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function test_query1($link)
{
  env_var_for_expects("GUID_TEST_MYSQLI_Q1", newrelic_get_linking_metadata()['span.id'] ?? '');
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'";
  $result = $link->query($query);
  tap_not_equal(FALSE, $result, "test_query1 (query)");

  $expected = array(0 => "STATISTICS", "TABLE_NAME" => "STATISTICS");

  if (FALSE !== $result) {
    while ($row = $result->fetch_array()) {
        tap_equal($expected, $row, "test_query1 (fetch)");
    }
    $result->close();
  }
}

function test_query2($link)
{
  env_var_for_expects("GUID_TEST_MYSQLI_Q2", newrelic_get_linking_metadata()['span.id'] ?? '');
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'";
  $result = $link->query($query, MYSQLI_USE_RESULT);
  tap_not_equal(FALSE, $result, "test_query2 (query)");

  $expected = array(0 => "STATISTICS", "TABLE_NAME" => "STATISTICS");
  Fiber::suspend();
  if (FALSE !== $result) {
    while ($row = $result->fetch_array()) {
      tap_equal($expected, $row, "test_query2 (fetch)");
    }
    $result->close();
  }
}

function test_mysqli()
{
  global $MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET;

  env_var_for_expects("GUID_TEST_MYSQLI", newrelic_get_linking_metadata()['span.id'] ?? '');
  $link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
  if ($link->connect_errno) {
    echo $link->connect_error . "\n";
    exit(1);
  }

  test_query1($link);
  $fiber_q2 = new Fiber('test_query2');
  Fiber::suspend();
  $fiber_q2->start($link);
  $fiber_q2->resume();
  $link->close();
}

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
$fiber_mysqli = new Fiber('test_mysqli');

$fiber_mysqli->start();
$fiber_a->start();
$fiber_mysqli->resume();
$fiber_a->resume();
