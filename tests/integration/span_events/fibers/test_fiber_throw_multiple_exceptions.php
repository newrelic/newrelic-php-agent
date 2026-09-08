<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test Fiber::throw with multiple exception types and complex error handling.
Tests throwing different exception types into suspended fibers and verifies
that span events correctly capture error details for each exception type.
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
    "events_seen": 6
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
        "name": "Custom\/error_handler_fiber",
        "guid": "ENV[GUID_ERROR_HANDLER]",
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
        "name": "Custom\/validation_step",
        "guid": "ENV[GUID_VALIDATION]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_ERROR_HANDLER]"
      },
      {},
      {
        "error.message": "Uncaught exception 'InvalidArgumentException' with message 'Invalid data format' in __FILE__:??",
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
        "name": "Custom\/timeout_fiber",
        "guid": "ENV[GUID_TIMEOUT]",
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
        "name": "Custom\/long_operation",
        "guid": "ENV[GUID_OPERATION]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_TIMEOUT]"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Operation timeout' in __FILE__:??",
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
        "name": "Custom\/cleanup_operation",
        "guid": "ENV[GUID_CLEANUP_OP]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_OPERATION]"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Error handler fiber started
Validation step started
Timeout fiber started
Long operation started
Validation error caught: Invalid data format
Cleanup performed
Timeout error caught: Operation timeout
All error scenarios tested
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

function validation_step() {
    echo "Validation step started\n";
    env_var_for_expects("GUID_VALIDATION", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 50000000);

    // Suspend for external validation
    $data = Fiber::suspend();

    // This should not execute due to thrown exception
    echo "Validation passed for: " . $data . "\n";
    return "validation success";
}

function error_handler_fiber() {
    echo "Error handler fiber started\n";
    env_var_for_expects("GUID_ERROR_HANDLER", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        return validation_step();
    } catch (InvalidArgumentException $e) {
        echo "Validation error caught: " . $e->getMessage() . "\n";
        return null;
    }
}

function cleanup_operation() {
    env_var_for_expects("GUID_CLEANUP_OP", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 30000000);
    echo "Cleanup performed\n";
}

function long_operation() {
    echo "Long operation started\n";
    env_var_for_expects("GUID_OPERATION", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        time_nanosleep(0, 100000000);

        // Simulate a long-running operation that might timeout
        Fiber::suspend();

        echo "Long operation completed\n";
        return "operation result";
    } finally {
        // Cleanup should always happen
        cleanup_operation();
    }
}

function timeout_fiber() {
    echo "Timeout fiber started\n";
    env_var_for_expects("GUID_TIMEOUT", newrelic_get_linking_metadata()['span.id'] ?? '');

    try {
        return long_operation();
    } catch (RuntimeException $e) {
        echo "Timeout error caught: " . $e->getMessage() . "\n";
        return null;
    }
}

// Test scenario 1: Invalid argument exception
$errorFiber = new Fiber('error_handler_fiber');
$errorFiber->start();

// Test scenario 2: Runtime exception with cleanup
$timeoutFiber = new Fiber('timeout_fiber');
$timeoutFiber->start();

// Throw validation error into first fiber
try {
    $errorFiber->throw(new InvalidArgumentException('Invalid data format'));
} catch (InvalidArgumentException $e) {
    echo "Unexpected exception in main: " . $e->getMessage() . "\n";
}

// Throw timeout error into second fiber
try {
    $timeoutFiber->throw(new RuntimeException('Operation timeout'));
} catch (RuntimeException $e) {
    echo "Unexpected exception in main: " . $e->getMessage() . "\n";
}

echo "All error scenarios tested\n";
