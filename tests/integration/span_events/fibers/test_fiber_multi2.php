<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test should show proper parentage of txns with fiber activity.
Correct spans and parenting should be evident even with multiple fibers that called using
a producer/consumer pattern.
Note: 
consumer-a meets it's max and finishes. 
consumer-b doesn't meet the max so it keeps suspending/resuming until it is destroyed when the test ends.
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
Supportability/api/get_linking_metadata, 9
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
      "name": "Custom\/FiberQueue::addProducer",
      "guid": "ENV[GUID_ADD_PRODUCER_slow-producer]",
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
      "name": "??closure:FiberQueue::addProducer??",
      "guid": "ENV[GUID_FIBER_slow-producer]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_PRODUCER_slow-producer]"
    },
    {
      "fiber_end_slow-producer": "ENV[GUID_FIBER_NAME_slow-producer]",
      "fiber_start_slow-producer": "ENV[GUID_FIBER_NAME_slow-producer]"
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
      "name": "Custom\/FiberQueue::addProducer",
      "guid": "ENV[GUID_ADD_PRODUCER_fast-producer]",
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
      "name": "??closure:FiberQueue::addProducer??",
      "guid": "ENV[GUID_FIBER_fast-producer]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_PRODUCER_fast-producer]"
    },
    {
      "fiber_end_fast-producer": "ENV[GUID_FIBER_NAME_fast-producer]",
      "fiber_start_fast-producer": "ENV[GUID_FIBER_NAME_fast-producer]"
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
      "name": "Custom\/FiberQueue::addConsumer",
      "guid": "ENV[GUID_ADD_CONSUMER_consumer-a]",
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
      "name": "??closure:FiberQueue::addConsumer??",
      "guid": "ENV[GUID_FIBER_consumer-a]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_CONSUMER_consumer-a]"
    },
    {
      "fiber_end_consumer-a": "ENV[GUID_FIBER_NAME_consumer-a]",
      "fiber_start_consumer-a": "ENV[GUID_FIBER_NAME_consumer-a]"
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
      "name": "Custom\/FiberQueue::addConsumer",
      "guid": "ENV[GUID_ADD_CONSUMER_consumer-b]",
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
      "name": "??closure:FiberQueue::addConsumer??",
      "guid": "ENV[GUID_FIBER_consumer-b]",
      "timestamp": "??",
      "duration": "??",
      "category": "generic",
      "parentId": "ENV[GUID_ADD_CONSUMER_consumer-b]"
    },
    {
      "fiber_end_consumer-b": "ENV[GUID_FIBER_NAME_consumer-b]",
      "fiber_start_consumer-b": "ENV[GUID_FIBER_NAME_consumer-b]"
    },
    {}
  ]
]
*/

/*EXPECT
=== Producer-Consumer Pattern ===
Producer fast-producer created: fast-producer-item-0
Producer slow-producer created: slow-producer-item-0

--- Processing Round 0 ---
Producer fast-producer created: fast-producer-item-1
Producer slow-producer created: slow-producer-item-1
Consumer consumer-a processed: fast-producer-item-1
Consumer consumer-b processed: slow-producer-item-1

--- Processing Round 1 ---
Producer fast-producer created: fast-producer-item-2
PRODUCER slow-producer COMPLETED
Consumer consumer-a processed: fast-producer-item-2
CONSUMER consumer-a COMPLETED

--- Processing Round 2 ---
PRODUCER fast-producer COMPLETED

--- Processing Round 3 ---

--- Processing Round 4 ---

--- Processing Round 5 ---

--- Processing Round 6 ---

--- Processing Round 7 ---
multi-fiber completed
*/

/*EXPECT_ERROR_EVENTS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');


env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');
            
// Test fiber-based producer-consumer pattern
class FiberQueue {
    private $items = [];
    private $producers = [];
    private $consumers = [];

    public function addProducer($name, $itemCount) {
      env_var_for_expects("GUID_ADD_PRODUCER_" . $name, newrelic_get_linking_metadata()['span.id'] ?? '');
      time_nanosleep(0, 100000000);
      $fiber = new Fiber(function() use ($name, $itemCount) {
        env_var_for_expects("GUID_FIBER_" . $name, newrelic_get_linking_metadata()['span.id'] ?? '');
        newrelic_add_custom_span_parameter("fiber_start_" . $name, $name);
        
          for ($i = 0; $i < $itemCount; $i++) {
              $item = "$name-item-$i";
              echo "Producer $name created: $item" . PHP_EOL;
              Fiber::suspend($item);

          }

          env_var_for_expects("GUID_FIBER_NAME_" . $name, $name);
          newrelic_add_custom_span_parameter("fiber_end_" . $name, $name);
          return "Producer $name finished";
      });

      $this->producers[$name] = $fiber;
      return $fiber->start();
    }

    public function addConsumer($name, $maxItems) {
        env_var_for_expects("GUID_ADD_CONSUMER_" . $name, newrelic_get_linking_metadata()['span.id'] ?? '');
        time_nanosleep(0, 100000000);

        $fiber = new Fiber(function() use ($name, $maxItems) {
          env_var_for_expects("GUID_FIBER_" . $name, newrelic_get_linking_metadata()['span.id'] ?? '');
          newrelic_add_custom_span_parameter("fiber_start_" . $name, $name);
          try {
            $consumed = 0;
            while ($consumed < $maxItems - 1) {
                $item = Fiber::suspend("waiting-for-item");
                if ($item !== null) {
                    echo "Consumer $name processed: $item" . PHP_EOL;
                    $consumed++;
                }
            }
          } finally {
            env_var_for_expects("GUID_FIBER_NAME_" . $name, $name);
            newrelic_add_custom_span_parameter("fiber_end_" . $name, $name);
          }
            return "Consumer $name finished";
        });

        $this->consumers[$name] = $fiber;
        return $fiber->start();
    }

    public function processQueue($rounds) {
        for ($round = 0; $round < $rounds; $round++) {
            echo "\n--- Processing Round $round ---" . PHP_EOL;

            // Get items from producers
            foreach ($this->producers as $name => $fiber) {
                if (!$fiber->isTerminated()) {
                    $item = $fiber->resume();
                    if (!$fiber->isTerminated()) {
                        $this->items[] = $item;
                    } else {
                        echo "PRODUCER $name COMPLETED" . PHP_EOL;
                    }
                }
            }

            // Give items to consumers
            foreach ($this->consumers as $name => $fiber) {
                if (!$fiber->isTerminated()) {
                   if (!empty($this->items)) {
                       $item = array_shift($this->items);
                       $result = $fiber->resume($item);
                       if ($fiber->isTerminated()) {
                           echo "CONSUMER $name COMPLETED" . PHP_EOL;
                       }
                   } else {
                       $fiber->resume(null);
                   }
                }
            }

            // Feed consumers with null if no items
            foreach ($this->consumers as $name => $fiber) {
                if (!$fiber->isTerminated()) {
                    $fiber->resume(null);
                }
            }
        }
    }
}

// Run producer-consumer multi-fiber
echo "\n=== Producer-Consumer Pattern ===" . PHP_EOL;
$queue = new FiberQueue();
$queue->addProducer('fast-producer', 3);
$queue->addProducer('slow-producer', 2);
$queue->addConsumer('consumer-a', 3);
$queue->addConsumer('consumer-b', 3);

$queue->processQueue(8);

echo "multi-fiber completed" . PHP_EOL;
