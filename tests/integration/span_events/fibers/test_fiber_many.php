<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity.
Correct spans and parenting should be evident even with multiple fibers.
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
--- Many Fibers Test: 200 fibers ---
Creating 200 fibers with 5 iterations each...
Fiber creation time: .*
Fiber start time: .*
Total execution time: .*
Terminated fibers: 200, Active: 0, Total sum: 0
Average time per fiber: .*
Throughput: .*
*/

/*EXPECT_ERROR_EVENTS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');


// Many test with many fibers
function manyTest($fiberCount, $iterations) {
    echo "Creating $fiberCount fibers with $iterations iterations each..." . PHP_EOL;

    env_var_for_expects("GUID_MANY_TEST", newrelic_get_linking_metadata()['span.id'] ?? '');

    $startTime = microtime(true);
    $fibers = [];

    // Create all fibers
    for ($i = 0; $i < $fiberCount; $i++) {
        $fibers[] = new Fiber(function() use ($i, $iterations) {
            env_var_for_expects("GUID_FIBER_NUM_" . $i, newrelic_get_linking_metadata()['span.id'] ?? '');
            $sum = 0;
            for ($j = 0; $j < $iterations; $j++) {
                $value = Fiber::suspend($i * 1000 + $j);
                $sum += (int)$value;
            }
            env_var_for_expects("GUID_FIBER_NUM_INT_" . $i, $i);
            newrelic_add_custom_span_parameter("fibernum_" . $i, $i);
            return $sum;
        });
    }

    $creationTime = microtime(true);
    echo "Fiber creation time: " . number_format(($creationTime - $startTime) * 1000, 2) . "ms" . PHP_EOL;

    // Start all fibers
    $results = [];
    foreach ($fibers as $index => $fiber) {
        $results[$index] = $fiber->start();
    }

    $startExecutionTime = microtime(true);
    echo "Fiber start time: " . number_format(($startExecutionTime - $creationTime) * 1000, 2) . "ms" . PHP_EOL;

    // Resume fibers multiple times
    for ($iteration = 0; $iteration < $iterations; $iteration++) {
        foreach ($fibers as $index => $fiber) {
            if (!$fiber->isTerminated()) {
                $results[$index] = $fiber->resume($iteration + 1);
            }
        }
    }

    $endTime = microtime(true);
    echo "Total execution time: " . number_format(($endTime - $startTime) * 1000, 2) . "ms" . PHP_EOL;

    // Verify results
    $activeCount = 0;
    $terminatedCount = 0;
    $totalSum = 0;

    foreach ($fibers as $index => $fiber) {
        if ($fiber->isTerminated()) {
            $terminatedCount++;
            $totalSum += $results[$index];
        } else {
            $activeCount++;
        }
    }

    echo "Terminated fibers: $terminatedCount, Active: $activeCount, Total sum: $totalSum" . PHP_EOL;
    return $endTime - $startTime;
}

// Run tests with different fiber counts
$testSizes = [200];
$iterations = 5;

foreach ($testSizes as $size) {
    echo "\n--- Many Fibers Test: $size fibers ---" . PHP_EOL;
    $time = manyTest($size, $iterations);
    echo "Average time per fiber: " . number_format(($time * 1000) / $size, 3) . "ms" . PHP_EOL;
    echo "Throughput: " . number_format($size / $time, 0) . " fibers/second" . PHP_EOL;
}
