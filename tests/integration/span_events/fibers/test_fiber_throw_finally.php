<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test Fiber::throw with finally blocks for proper cleanup.
Tests that when an exception is thrown into a suspended fiber, finally blocks
are executed correctly and span events are properly recorded during cleanup.
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
        "name": "Custom\/fiber_with_cleanup",
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
        "name": "Custom\/critical_section",
        "guid": "ENV[GUID_CRITICAL]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_FIBER]"
      },
      {},
      {
        "error.message": "Uncaught exception 'Exception' with message 'Emergency shutdown' in __FILE__:??",
        "error.class": "Exception"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/cleanup_resources",
        "guid": "ENV[GUID_CLEANUP]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_CRITICAL]"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Fiber started
Entering critical section
Cleanup executed in finally block
Exception handled: Emergency shutdown
Program cleanup complete
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function cleanup_resources() {
    env_var_for_expects("GUID_CLEANUP", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 50000000);
    echo "Cleanup executed in finally block\n";
}

function critical_section() {
    echo "Entering critical section\n";
    env_var_for_expects("GUID_CRITICAL", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        time_nanosleep(0, 100000000);

        // Suspend in the critical section
        Fiber::suspend();

        // This would normally do important work
        echo "Critical work completed\n";
        return "success";
    } finally {
        // This should always execute, even when exception is thrown
        cleanup_resources();
    }
}

function fiber_with_cleanup() {
    echo "Fiber started\n";
    env_var_for_expects("GUID_FIBER", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        return critical_section();
    } catch (Exception $e) {
        echo "Exception handled: " . $e->getMessage() . "\n";
        return null;
    }
}

// Create and start the fiber
$fiber = new Fiber('fiber_with_cleanup');
$fiber->start();

// Throw an emergency shutdown exception
try {
    $fiber->throw(new Exception('Emergency shutdown'));
} catch (Exception $e) {
    // This should not be reached since exception is caught in fiber
    echo "Unexpected exception: " . $e->getMessage() . "\n";
}

echo "Program cleanup complete\n";