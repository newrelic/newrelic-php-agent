<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity.
Correct spans and parenting should be evident even with multiple fibers that consume memory.
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

/*EXPECT_METRICS_EXIST
Supportability/api/get_linking_metadata, 202
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
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "Custom\/manyTest",
      "guid": "ENV[GUID_MANY_TEST]",
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
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_0]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_0": ENV[GUID_FIBER_NUM_INT_0]
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_11]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_11": ENV[GUID_FIBER_NUM_INT_11]
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_21]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_21": ENV[GUID_FIBER_NUM_INT_21]
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_51]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_51": ENV[GUID_FIBER_NUM_INT_51]
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_101]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_101": ENV[GUID_FIBER_NUM_INT_101]
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_133]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_133": ENV[GUID_FIBER_NUM_INT_133]
    },
    {}
  ],
  [
    {
      "type": "Span",
      "traceId": "??",
      "transactionId": "??",
      "sampled": true,
      "priority": "??",
      "name": "??closure:manyTest??",
      "guid": "ENV[GUID_FIBER_NUM_199]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_MANY_TEST]"
    },
    {
      "fibernum_199": ENV[GUID_FIBER_NUM_INT_199]
    },
    {}
  ]
]
*/

/*EXPECT_REGEX
Creating 200 fibers.

=== Memory Usage Test ===
Initial memory: .*
After fiber creation: .*
Memory per fiber: .*
After fiber start: .*
Terminated fibers: 200, Active: 0
Final memory: .*
Peak memory: .*
After cleanup: .*
*/

/*EXPECT_ERROR_EVENTS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');


// Many test with many fibers
function manyTest($fiberCount) {
    echo "Creating $fiberCount fibers." . PHP_EOL;

    env_var_for_expects("GUID_MANY_TEST", newrelic_get_linking_metadata()['span.id'] ?? '');

    // Memory usage test

    echo "\n=== Memory Usage Test ===" . PHP_EOL;
    $initialMemory = memory_get_usage();
    $initialPeakMemory = memory_get_peak_usage();

    echo "Initial memory: " . number_format($initialMemory / 1024) . " KB" . PHP_EOL;

    $heavyFibers = [];
    for ($i = 0; $i < $fiberCount; $i++) {
        $heavyFibers[] = new Fiber(function() use ($i) {
            env_var_for_expects("GUID_FIBER_NUM_" . $i, newrelic_get_linking_metadata()['span.id'] ?? '');

            // Create some data in fiber
            $data = array_fill(0, 1000, "fiber-$i-data-" . str_repeat('x', 100));
            Fiber::suspend(count($data));

            // Process data
            $processed = array_map(function($item) {
                return hash('sha256', $item);
            }, $data);

            env_var_for_expects("GUID_FIBER_NUM_INT_" . $i, $i);
            newrelic_add_custom_span_parameter("fibernum_" . $i, $i);


            return count($processed);
        });
    }

    $afterCreationMemory = memory_get_usage();
    echo "After fiber creation: " . number_format($afterCreationMemory / 1024) . " KB" . PHP_EOL;
    echo "Memory per fiber: " . number_format(($afterCreationMemory - $initialMemory) / count($heavyFibers)) . " bytes" . PHP_EOL;

    // Start and run heavy fibers
    foreach ($heavyFibers as $fiber) {
        $fiber->start();
    }

    $afterStartMemory = memory_get_usage();
    echo "After fiber start: " . number_format($afterStartMemory / 1024) . " KB" . PHP_EOL;

    foreach ($heavyFibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    // Verify results
    $activeCount = 0;
    $terminatedCount = 0;

    foreach ($heavyFibers as $index => $fiber) {
        if ($fiber->isTerminated()) {
            $terminatedCount++;
        } else {
            $activeCount++;
        }
    }

    echo "Terminated fibers: $terminatedCount, Active: $activeCount" . PHP_EOL;

    $finalMemory = memory_get_usage();
    $finalPeakMemory = memory_get_peak_usage();

    echo "Final memory: " . number_format($finalMemory / 1024) . " KB" . PHP_EOL;
    echo "Peak memory: " . number_format($finalPeakMemory / 1024) . " KB" . PHP_EOL;

    // Cleanup test
    unset($heavyFibers);
    gc_collect_cycles();

    $cleanupMemory = memory_get_usage();
    echo "After cleanup: " . number_format($cleanupMemory / 1024) . " KB" . PHP_EOL;

}

manyTest(200);
