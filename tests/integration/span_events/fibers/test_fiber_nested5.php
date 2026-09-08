<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity.
The nested version here resumes all fibers gracefully.
Output should show that PHP functionality should continue to work as expected.
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
        "name": "??closure:createNestedFiber??",
        "guid": "ENV[GUID_LEVEL_1]",
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
        "name": "??closure:createNestedFiber??",
        "guid": "ENV[GUID_LEVEL_2]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_LEVEL_1]"
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
        "name": "??closure:createNestedFiber??}",
        "guid": "ENV[GUID_LEVEL_3]",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "ENV[GUID_LEVEL_2]"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Fiber level 1 starting
Fiber level 2 starting
Fiber level 3 starting
Nested fiber level 2 received: suspended-level-3
Nested fiber level 1 received: suspended-level-2
Main received from fiber: suspended-level-1
Fiber level 1 resuming
Fiber level 1 will resume the suspended nested fiber
Fiber level 2 resuming
Fiber level 2 will resume the suspended nested fiber
Fiber level 3 resuming
Final result: completed-level-1
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

// Test nested fibers
function createNestedFiber($level, $maxLevel) {
    return new Fiber(function() use ($level, $maxLevel) {
      echo "Fiber level $level starting" . PHP_EOL;
      time_nanosleep(0, 100000000);
      env_var_for_expects("GUID_LEVEL_" . $level, newrelic_get_linking_metadata()['span.id'] ?? '');
      if ($level < $maxLevel) {
          // Create a nested fiber
          $nestedFiber = createNestedFiber($level + 1, $maxLevel);
          $nestedResult = $nestedFiber->start();
          echo "Nested fiber level $level received: $nestedResult" . PHP_EOL;
      }

      Fiber::suspend("suspended-level-$level");
      echo "Fiber level $level resuming" . PHP_EOL;

      if ($level < $maxLevel) { 
        echo "Fiber level $level will resume the suspended nested fiber" . PHP_EOL;
        $nestedFiber->resume();
      }
      return "completed-level-$level";
    });
}

// Create and run nested fibers
$mainFiber = createNestedFiber(1, 3);
$result = $mainFiber->start();
echo "Main received from fiber: $result" . PHP_EOL;
$mainFiber->resume();
$finalResult = $mainFiber->getReturn();
echo "Final result: $finalResult" . PHP_EOL;
