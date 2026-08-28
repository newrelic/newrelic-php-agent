<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity.
Correct spans and parenting should be evident even with multiple fibers that called from a class.
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
Supportability/api/get_linking_metadata, 10
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
      "name": "Custom\/MultiOperation::addOperation",
      "guid": "ENV[GUID_ADD_OP_database-query]",
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
      "name": "??closure:MultiOperation::addOperation??",
      "guid": "ENV[GUID_FIBER_OP_database-query]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_OP_database-query]"
    },
    {
      "fiberop_end_database-query": "ENV[GUID_FIBER_OP_NAME_database-query]",
      "fiberop_start_database-query": "ENV[GUID_FIBER_OP_NAME_database-query]"
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
      "name": "Custom\/MultiOperation::addOperation",
      "guid": "ENV[GUID_ADD_OP_file-upload]",
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
      "name": "??closure:MultiOperation::addOperation??",
      "guid": "ENV[GUID_FIBER_OP_file-upload]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_OP_file-upload]"
    },
    {
      "fiberop_end_file-upload": "ENV[GUID_FIBER_OP_NAME_file-upload]",
      "fiberop_start_file-upload": "ENV[GUID_FIBER_OP_NAME_file-upload]"
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
      "name": "Custom\/MultiOperation::addOperation",
      "guid": "ENV[GUID_ADD_OP_api-call]",
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
      "name": "??closure:MultiOperation::addOperation??",
      "guid": "ENV[GUID_FIBER_OP_api-call]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_OP_api-call]"
    },
    {
      "fiberop_end_api-call": "ENV[GUID_FIBER_OP_NAME_api-call]",
      "fiberop_start_api-call": "ENV[GUID_FIBER_OP_NAME_api-call]"
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
      "name": "Custom\/MultiOperation::addOperation",
      "guid": "ENV[GUID_ADD_OP_image-processing]",
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
      "name": "??closure:MultiOperation::addOperation??",
      "guid": "ENV[GUID_FIBER_OP_image-processing]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_OP_image-processing]"
    },
    {
      "fiberop_end_image-processing": "ENV[GUID_FIBER_OP_NAME_image-processing]",
      "fiberop_start_image-processing": "ENV[GUID_FIBER_OP_NAME_image-processing]"
    },
    {}
  ]
]
*/

/*EXPECT
Starting operation: database-query
Starting operation: file-upload
Starting operation: api-call
Starting operation: image-processing

--- Tick 1 ---
Operation database-query progress: progress-1
Operation file-upload progress: progress-1
Operation api-call progress: progress-1
Operation image-processing progress: progress-1

--- Tick 2 ---
Operation database-query progress: progress-2
Operation file-upload progress: progress-2
Operation api-call progress: progress-2
COMPLETED: 
Operation image-processing progress: progress-2

--- Tick 3 ---
Operation database-query progress: progress-3
COMPLETED: 
Operation file-upload progress: progress-3
Operation image-processing progress: progress-3

--- Tick 4 ---
Operation file-upload progress: progress-4
Operation image-processing progress: progress-4
COMPLETED: 

--- Tick 5 ---
Operation file-upload progress: progress-5
COMPLETED: 

All operations completed!
*/

/*EXPECT_ERROR_EVENTS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

// Simulate multiple operations with multiple fibers
class MultiOperation {
    private $operations = [];

    public function addOperation($name, $duration) {
        env_var_for_expects("GUID_ADD_OP_" . $name, newrelic_get_linking_metadata()['span.id'] ?? '');
        time_nanosleep(0, 100000000);
        $fiber = new Fiber(function() use ($name, $duration) {
            env_var_for_expects("GUID_FIBER_OP_" . $name, newrelic_get_linking_metadata()['span.id'] ?? '');
            echo "Starting operation: $name" . PHP_EOL;
            newrelic_add_custom_span_parameter("fiberop_start_" . $name, $name);

            // Simulate multi fiber work with multiple suspend points
            for ($i = 0; $i < $duration; $i++) {
                $progress = Fiber::suspend("$name-step-$i");
                echo "Operation $name progress: $progress" . PHP_EOL;
            }

            env_var_for_expects("GUID_FIBER_OP_NAME_" . $name, $name);
            newrelic_add_custom_span_parameter("fiberop_end_" . $name, $name);

            return "Operation $name completed";
        });

        $this->operations[$name] = [
            'fiber' => $fiber,
            'duration' => $duration,
            'current_step' => 0
        ];

        return $fiber->start(); // Start immediately
    }

    public function runOperations() {
        $completed = 0;
        $total = count($this->operations);
        $tick = 0;

        env_var_for_expects("GUID_RUN_OP", newrelic_get_linking_metadata()['span.id'] ?? '');

        while ($completed < $total) {
            $tick++;
            echo "\n--- Tick $tick ---" . PHP_EOL;

            foreach ($this->operations as $name => &$op) {
                if ($op['fiber']->isTerminated()) {
                    continue;
                }

                if ($op['current_step'] < $op['duration']) {
                    $result = $op['fiber']->resume("progress-" . ($op['current_step'] + 1));
                    $op['current_step']++;

                    if ($op['fiber']->isTerminated()) {
                        echo "COMPLETED: $result" . PHP_EOL;
                        $completed++;
                    }
                }
            }
            unset($op);

            // Simulate processing delay
            usleep(100000); // 0.1 seconds
        }

        echo "\nAll operations completed!" . PHP_EOL;
    }
}

// Run
$multi = new MultiOperation();
$multi->addOperation('database-query', 3);
$multi->addOperation('file-upload', 5);
$multi->addOperation('api-call', 2);
$multi->addOperation('image-processing', 4);

$multi->runOperations();
