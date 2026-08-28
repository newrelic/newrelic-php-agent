<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test Fiber::throw behavior with different fiber states.
Tests throwing exceptions into fibers in various states and ensures proper
span event recording and error handling for each scenario. Since all exceptions
are caught, no errors will show.
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
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/suspending_fiber",
        "guid": "ENV[GUID_SUSPENDING]",
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
        "name": "Custom\/completing_fiber",
        "guid": "ENV[GUID_COMPLETING]",
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
        "name": "Custom\/quick_task",
        "guid": "ENV[GUID_QUICK]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_COMPLETING]"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Suspending fiber started
Completing fiber started
Quick task executed
Completing fiber finished
Exception in suspending fiber: Interrupted during suspension
Testing fiber states completed
Cannot throw into terminated fiber
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function quick_task() {
    env_var_for_expects("GUID_QUICK", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 50000000);
    echo "Quick task executed\n";
    return "quick result";
}

function suspending_fiber() {
    echo "Suspending fiber started\n";
    env_var_for_expects("GUID_SUSPENDING", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);

    try {
        // This fiber will suspend and wait
        Fiber::suspend();

        // Should not reach this due to thrown exception
        echo "Suspending fiber resumed\n";
        return "suspension result";
    } catch (UnexpectedValueException $e) {
        echo "Exception in suspending fiber: " . $e->getMessage() . "\n";
        return null;
    }
}

function completing_fiber() {
    echo "Completing fiber started\n";
    env_var_for_expects("GUID_COMPLETING", newrelic_get_linking_metadata()['span.id'] ?? '');

    // This fiber will complete quickly without suspending
    quick_task();
    time_nanosleep(0, 75000000);

    echo "Completing fiber finished\n";
    return "completed";
}

// Test 1: Create a fiber that will suspend
$suspendingFiber = new Fiber('suspending_fiber');
$suspendingFiber->start();

// Test 2: Create a fiber that will complete without suspending
$completingFiber = new Fiber('completing_fiber');
$result = $completingFiber->start();

// Test 3: Throw into the suspended fiber (should work)
try {
    $suspendingFiber->throw(new UnexpectedValueException('Interrupted during suspension'));
} catch (UnexpectedValueException $e) {
    // Should not reach here since exception is caught in fiber
    echo "Unexpected exception in main: " . $e->getMessage() . "\n";
}

echo "Testing fiber states completed\n";

// Test 4: Try to throw into a terminated fiber (should fail)
try {
    $completingFiber->throw(new Exception('Should not work'));
} catch (FiberError $e) {
    echo "Cannot throw into terminated fiber\n";
} catch (Exception $e) {
    echo "Unexpected exception type: " . $e->getMessage() . "\n";
}
