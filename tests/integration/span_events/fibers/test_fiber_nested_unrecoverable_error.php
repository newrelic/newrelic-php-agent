<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity even when an unrecoverable error occurs.
Fibers that led to the unrecoverable error should be named when the txn abruptly ends.
Fibers that were otherwise open will be named "<unknown>" when the txn abruptly ends.
Output should show that PHP functionality does not continue to work as expected and ends execution due 
to the unrecoverable error.
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
      {
        "error.message": "Uncaught exception 'Error' with message 'Call to undefined function func_does_not_exist()' in __FILE__:??",
        "error.class": "Error"
      }
    ],
    [
      {
       "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "<unknown>",
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
      {
        "error.message": "Uncaught exception 'Error' with message 'Call to undefined function func_does_not_exist()' in __FILE__:??",
        "error.class": "Error"
      }
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
        "error.message": "Uncaught exception 'Error' with message 'Call to undefined function func_does_not_exist()' in __FILE__:??",
        "error.class": "Error"
      }
    ]
  ]
]
*/

/*EXPECT_REGEX
Starting Func 'a'
Starting Func 'b'
Starting Func 'c'

Fatal error:.*
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
    echo func_does_not_exist(0) . "\n"; // This will throw an unrecoverable error
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
