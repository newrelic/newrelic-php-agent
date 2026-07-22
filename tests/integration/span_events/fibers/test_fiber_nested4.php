<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity.

Output should show that PHP functionality should continue to work as expected.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP 8.1+ required\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.fibers.disabled = false
newrelic.special = show_fibers, show_executes
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
       "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/a",
        "guid": "ENV[GUID_A]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_ROOT]"
      },
      {},
      {}
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/b",
        "guid": "ENV[GUID_B]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_ROOT]"
      },
      {},
      {}
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/c",
        "guid": "ENV[GUID_C]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_B]"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
        "error.class": "RuntimeException"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/fraction",
        "guid": "ENV[GUID_FRACTION]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_C]"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
        "error.class": "RuntimeException"
      }
    ]
  ]
]
*/

/*EXPECT
Starting Func 'a'
Starting Func 'b'
Starting Func 'c'
Starting Func 'fraction'
Caught exception: Division by zero
Ending Func 'b'
Ending Func 'a'
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function c()
{
    echo "Starting Func 'c'\n";
    env_var_for_expects("GUID_C", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo fraction(0) . "\n";
    echo "Ending Func 'c'\n";
}

function b()
{
    echo "Starting Func 'b'\n";
    env_var_for_expects("GUID_B", newrelic_get_linking_metadata()['span.id'] ?? '');
    $fiberc = new Fiber('c');
    Fiber::suspend();
    try {
        $fiberc->start();
        time_nanosleep(0, 100000000);
        $fiberc->resume();
    } catch (RuntimeException $e) {
        echo("Caught exception: " . $e->getMessage() . "\n");
    }
    echo "Ending Func 'b'\n";
}

function fraction($x) {
    echo "Starting Func 'fraction'\n";
    env_var_for_expects("GUID_FRACTION", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    if (!$x) {
        throw new RuntimeException('Division by zero');
    }
    echo "Ending Func 'fraction'\n";
    return 1/$x;
}

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    Fiber::suspend();
    time_nanosleep(0, 100000000);
    echo "Ending Func 'a'\n";
};

$fibera = new Fiber('a');
$fiberb = new Fiber('b');

$fibera->start();
$fiberb->start();
$fiberb->resume();
$fibera->resume();
