<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test Fiber::throw with nested fibers and complex span hierarchies.
Tests that when exceptions are thrown into nested suspended fibers, the span
hierarchy is correctly maintained and error propagation works as expected.
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
        "name": "Custom\/coordinator_fiber",
        "guid": "ENV[GUID_COORDINATOR]",
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
        "name": "Custom\/worker_fiber",
        "guid": "ENV[GUID_WORKER]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_COORDINATOR]"
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
        "name": "Custom\/work_process",
        "guid": "ENV[GUID_WORK_PROCESS]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_WORKER]"
      },
      {},
      {
        "error.message": "Uncaught exception 'LogicException' with message 'Worker termination requested' in __FILE__:??",
        "error.class": "LogicException"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/status_check",
        "guid": "ENV[GUID_STATUS]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_COORDINATOR]"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Coordinator started
Worker fiber started
Work process initiated
Coordinator suspended, worker running
Coordinator resumed
Worker terminated with: Worker termination requested
Status check completed
Coordination completed successfully
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function work_process() {
    echo "Work process initiated\n";
    env_var_for_expects("GUID_WORK_PROCESS", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 75000000);

    // Worker suspends here waiting for more work
    $task = Fiber::suspend();

    // This should never execute due to thrown exception
    echo "Processing task: " . $task . "\n";
    return "task completed";
}

function worker_fiber() {
    echo "Worker fiber started\n";
    env_var_for_expects("GUID_WORKER", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        return work_process();
    } catch (LogicException $e) {
        echo "Worker terminated with: " . $e->getMessage() . "\n";
        return null;
    }
}

function status_check() {
    env_var_for_expects("GUID_STATUS", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 50000000);
    echo "Status check completed\n";
    return "all systems operational";
}

function coordinator_fiber() {
    echo "Coordinator started\n";
    env_var_for_expects("GUID_COORDINATOR", newrelic_get_linking_metadata()['span.id'] ?? '');

    // Start a worker fiber
    $worker = new Fiber('worker_fiber');
    $worker->start();

    echo "Coordinator suspended, worker running\n";

    // Suspend coordinator while worker is running
    Fiber::suspend();

    echo "Coordinator resumed\n";


    // Throw exception into the worker
    try {
        $worker->throw(new LogicException('Worker termination requested'));
    } catch (LogicException $e) {
        // Should not reach here since worker catches it
        echo "Unexpected exception: " . $e->getMessage() . "\n";
    }

    // Check if worker is still alive (it shouldn't be after exception)
    if ($worker->isTerminated()) {
        // Do status check
        status_check();
    }

    return "coordination completed";
}

// Create and start the coordinator fiber
$coordinator = new Fiber('coordinator_fiber');
$coordinator->start();

// Let some time pass to simulate work
time_nanosleep(0, 100000000);

// Resume the coordinator
if (!$coordinator->isTerminated()) {
    $coordinator->resume();
}

echo "Coordination completed successfully\n";
