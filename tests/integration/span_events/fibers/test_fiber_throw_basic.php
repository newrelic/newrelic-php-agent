<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test basic Fiber::throw functionality with span events.
Tests that when an exception is thrown into a suspended fiber, the span hierarchy
is correctly maintained and error information is captured.
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
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 3
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
        "name": "Custom\/fiber_function",
        "guid": "ENV[GUID_FIBER]",
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
        "name": "Custom\/work_function",
        "guid": "ENV[GUID_WORK]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_FIBER]"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Thrown into fiber' in __FILE__:??",
        "error.class": "RuntimeException"
      }
    ]
  ]
]
*/

/*EXPECT
Starting fiber function
Starting work function
Caught exception: Thrown into fiber
Main program continues
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function work_function() {
    echo "Starting work function\n";
    env_var_for_expects("GUID_WORK", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);

    // This suspend point is where the exception will be thrown from
    Fiber::suspend();

    // This should never execute
    echo "Work function resumed\n";
    return "work done";
}

function fiber_function() {
    echo "Starting fiber function\n";
    env_var_for_expects("GUID_FIBER", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        return work_function();
    } catch (RuntimeException $e) {
        echo "Caught exception: " . $e->getMessage() . "\n";
        return null;
    }
}

// Create and start the fiber
$fiber = new Fiber('fiber_function');
$fiber->start();

// Throw an exception into the suspended fiber
try {
    $fiber->throw(new RuntimeException('Thrown into fiber'));
} catch (RuntimeException $e) {
    // This should not be reached since the exception is caught inside the fiber
    echo "Unexpected exception in main: " . $e->getMessage() . "\n";
}

echo "Main program continues\n";
