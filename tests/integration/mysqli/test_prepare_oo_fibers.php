<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for mysqli::prepare if fibers are involved.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.explain_enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT
ok - execute select 3*0
ok - iteration  0
Starting Func 'a'
ok - execute select 3*1
ok - iteration  1
Ending Func 'a'
ok - execute select 3*2
ok - iteration  2
ok - execute select 3*3
ok - iteration  3
ok - execute select 3*4
ok - iteration  4
ok - execute select 3*5
ok - iteration  5
ok - execute select 3*6
ok - iteration  6
ok - execute select 3*7
ok - iteration  7
ok - execute select 3*8
ok - iteration  8
ok - execute select 3*9
ok - iteration  9
ok - execute select 3*10
ok - iteration 10
ok - execute select 3*11
ok - iteration 11
ok - execute select 3*12
ok - iteration 12
ok - execute select 3*13
ok - iteration 13
ok - execute select 3*14
ok - iteration 14
ok - execute select 3*15
ok - iteration 15
*/

/*EXPECT_SPAN_EVENTS_LIKE
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
      "name": "Custom\/test_prepare_oo",
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
      "name": "Datastore\/operation\/MySQL\/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_PREPARE]",
      "span.kind": "client",
      "component": "MySQL"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "select ?*?"
    }
  ],
    [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/MySQL\/select",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_PREPARE]",
      "span.kind": "client",
      "component": "MySQL"
    },
    {},
    {
      "peer.address": "unknown:unknown",
      "db.statement": "select ?*?"
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
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},                     [16, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select",
      "scope":"OtherTransaction/php__FILE__"},                        [16, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [16, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [16, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                  [16, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                             [16, "??", "??", "??", "??", "??"]],
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

function test_prepare_oo()
{
  global $MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET;

  env_var_for_expects("GUID_TEST_PREPARE", newrelic_get_linking_metadata()['span.id'] ?? '');
  $N = 16;
  $queries = array();
  $mysqlis = array();

  for ($i = 0; $i < $N; $i++) {
    /*
     * Note: Each iteration of this loop creates a new connection.
     */
    $query = "select 3*" . $i;
    $queries[$i] = $query;

    $mysqlis[$i] = new mysqli ($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
    if (mysqli_connect_errno ()) {
      printf ("Connect failed: %s\n", mysqli_connect_error ());
      exit ();
    }

    /*
     * Note: Sometimes the prepare doesn't always work.
     */
    if ($stmt = $mysqlis[$i]->prepare ($queries[$i])) {
      $execute_result = $stmt->execute ();
      tap_assert ($execute_result, sprintf ("execute %s", $queries[$i]));
      if ($execute_result) {
        $stmt->bind_result ($value);
        $stmt->fetch ();
        tap_equal (3*$i, $value, sprintf("iteration %2d", $i));
      }
    }

      Fiber::suspend($N - $i);

  }

  for ($i = 0; $i < $N; $i++) {
    $mysqlis[$i]->close ();
  }
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
$fiber_prepare = new Fiber('test_prepare_oo');

$fiber_prepare->start();
$fiber_a->start();
$fiber_prepare->resume();
$fiber_a->resume();

do {
  $result = $fiber_prepare->resume();
} while ($result > 0);
