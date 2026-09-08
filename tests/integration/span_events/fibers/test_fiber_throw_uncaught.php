<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test Fiber::throw with uncaught exception propagation.
Tests that when an exception is thrown into a suspended fiber and not caught
within the fiber, it properly propagates back to the caller and span error
information is correctly captured.
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
      {
        "error.message": "Uncaught exception 'InvalidArgumentException' with message 'Fiber exception propagated' in __FILE__:??",
        "error.class": "InvalidArgumentException"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/processing_task",
        "guid": "ENV[GUID_TASK]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_FIBER]"
      },
      {},
      {
        "error.message": "Uncaught exception 'InvalidArgumentException' with message 'Fiber exception propagated' in __FILE__:??",
        "error.class": "InvalidArgumentException"
      }
    ]
  ]
]
*/

/*EXPECT
Fiber started
Processing task started
Exception caught in main: Fiber exception propagated
Main execution completed
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function processing_task() {
    echo "Processing task started\n";
    env_var_for_expects("GUID_TASK", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 50000000);

    // Suspend and wait for external input
    $data = Fiber::suspend();

    // This should not execute since an exception will be thrown
    echo "Processing data: " . $data . "\n";
    return "processed: " . $data;
}

function fiber_function() {
    echo "Fiber started\n";
    env_var_for_expects("GUID_FIBER", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 50000000);

    // No try-catch here, so exception will propagate
    return processing_task();
}

// Create and start the fiber
$fiber = new Fiber('fiber_function');
$fiber->start();

// Instead of resuming with data, throw an exception
try {
    $fiber->throw(new InvalidArgumentException('Fiber exception propagated'));
} catch (InvalidArgumentException $e) {
    echo "Exception caught in main: " . $e->getMessage() . "\n";
}

echo "Main execution completed\n";