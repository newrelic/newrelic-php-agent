<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Test that the txn is ended if PHP fiber use is detected. 
Fiber action detected in a function `c` called `b` called from root span after the successful
completion of another non-fiber function `a`.
Root span and `a` span should be named.  `b` and `c` span will be named <unknown>.

Then a new txn is started in a fiber after the fiber has resumed from a suspension.

Detect the fiber activity and end the txn. Because the txn starts in a function 
that then has fiber activity that ends that txn on fiber exit, 
only a root span is created for the new txn.

Output should show that PHP functionality should continue to work 
as expected.

Note: 
Fiber activity is defined as:
1) fiber->start which triggers 
 a) fiber init 
 b) fiber switch from calling context to fiber context
2) fiber->resume which triggers
 a) fiber switch from calling context to fiber context
3) fiber->suspend which triggers
 a) fiber switch from fiber context to calling context
4) fiber exits/completes which triggers
 a) fiber switch from fiber context to calling context
 b) fiber destroy
5) calling function exits without calling fiber->resume which triggers
 a) fiber switch from calling context to fiber context
 b) fiber switch from fiber context to calling context
 c) fiber destroy
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
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = true
*/

/*EXPECT_ERROR_EVENTS
null
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
        "name": "<unknown>",
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
        "name": "<unknown>",
        "guid": "ENV[GUID_C]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_B]"
      },
      {},
      {}
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/Custom\/manual_txn",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??",
        "transaction.name": "OtherTransaction\/Custom\/manual_txn"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Starting Func 'a'
Ending Func 'a'
Starting Func 'b'
Starting Func 'c'
Starting Func 'fraction'
Caught exception: Division by zero
Ending Func 'b'
Ending Func 'endit'
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function c()
{

    echo "Starting Func 'c'\n";
    env_var_for_expects("GUID_C", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    $fiber = new Fiber('fraction');
    $fiber->start(0);
    $fiber->resume();
    echo "Ending Func 'c'\n";
}

function b()
{
    echo "Starting Func 'b'\n";
    env_var_for_expects("GUID_B", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    try {
      c();
    } catch (RuntimeException $e) {
        echo("Caught exception: " . $e->getMessage() . "\n");
    }
    echo "Ending Func 'b'\n";
}

function fraction($x) {
    echo "Starting Func 'fraction'\n";
    env_var_for_expects("GUID_FRACTION", newrelic_get_linking_metadata()['span.id'] ?? '');
    Fiber::suspend();

    newrelic_start_transaction(ini_get("newrelic.appname"));
    newrelic_name_transaction("manual_txn");
    
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
    time_nanosleep(0, 100000000);
    echo "Ending Func 'a'\n";
};

function endit()
{
    echo "Ending Func 'endit'\n";
    env_var_for_expects("GUID_ENDIT", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
}

a();
b();
endit();
