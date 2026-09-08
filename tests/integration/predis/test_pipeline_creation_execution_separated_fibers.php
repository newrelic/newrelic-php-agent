<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Predis pipelines work correctly when pipelines are created with multiple calls
in two different fibers and the execution for those pipelines runs interleaved in two other fibers.
This demonstrates pipeline creation/execution separation across fiber boundaries.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
?>
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT
Fiber 1 (Creator A): Creating pipeline
Fiber 2 (Creator B): Creating pipeline
Fiber 3 (Executor A): Starting execution of pipeline A
Fiber 4 (Executor B): Starting execution of pipeline B
Fiber 1 (Creator A): Adding commands to pipeline
Fiber 2 (Creator B): Adding commands to pipeline
Fiber 1 (Creator A): Pipeline A ready for execution
Fiber 2 (Creator B): Pipeline B ready for execution
Fiber 3 (Executor A): Executing pipeline A
Fiber 3 (Executor A): Pipeline A executed with 5 results
Fiber 4 (Executor B): Executing pipeline B
Fiber 4 (Executor B): Pipeline B executed with 5 results
All pipeline operations completed successfully
*/

/*EXPECT_METRICS_EXIST
Datastore/Redis/all, 14
Datastore/operation/Redis/del, 4
Datastore/operation/Redis/get, 2
Datastore/operation/Redis/exists, 2
Datastore/operation/Redis/incrby, 2
Datastore/operation/Redis/set, 2
Datastore/operation/Redis/ping, 2
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
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/test_pipeline_creation_execution_separated",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ROOT]"
    },
    {},
    {}
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/incrby",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/ping",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/del",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/set",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/exists",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

// Global storage for pipelines and results
$pipelines = [];
$pipeline_results = [];
$cleanup_keys = [];

function pipeline_creator_fiber($fiber_id, $pipeline_id) {
    global $REDIS_HOST, $REDIS_PORT, $pipelines, $cleanup_keys;

    echo "Fiber $fiber_id (Creator $pipeline_id): Creating pipeline\n";

    $client = new Predis\Client(array(
        'host' => $REDIS_HOST,
        'port' => $REDIS_PORT,
    ));

    try {
        $client->connect();
    } catch (Exception $e) {
        die("skip: " . $e->getMessage() . "\n");
    }

    // Generate unique keys for this pipeline
    $key_prefix = "pipeline_{$pipeline_id}";
    $main_key = $key_prefix . "_main";
    $counter_key = $key_prefix . "_counter";

    // Store keys for cleanup
    $cleanup_keys[$pipeline_id] = [$main_key, $counter_key];

    // Create pipeline using fluent interface as shown in Predis documentation
    $pipeline = $client->pipeline();

    Fiber::suspend();

    echo "Fiber $fiber_id (Creator $pipeline_id): Adding commands to pipeline\n";

    // Add multiple commands to the pipeline
    // Following patterns from existing tests and Predis documentation
    $pipeline->ping();
    $pipeline->set($main_key, "value_for_{$pipeline_id}");
    $pipeline->get($main_key);
    $pipeline->incrby($counter_key, $pipeline_id === 'A' ? 10 : 20);
    $pipeline->exists($main_key);

    Fiber::suspend();

    // Store the prepared pipeline for execution by another fiber
    $pipelines[$pipeline_id] = [
        'pipeline' => $pipeline,
        'client' => $client
    ];

    echo "Fiber $fiber_id (Creator $pipeline_id): Pipeline $pipeline_id ready for execution\n";

    Fiber::suspend();
}

function pipeline_executor_fiber($fiber_id, $pipeline_id) {
    global $pipelines, $pipeline_results, $cleanup_keys;

    env_var_for_expects("GUID_TEST_BASIC", newrelic_get_linking_metadata()['span.id'] ?? '');

    echo "Fiber $fiber_id (Executor $pipeline_id): Starting execution of pipeline $pipeline_id\n";

    // Wait for the pipeline to be available
    while (!isset($pipelines[$pipeline_id])) {
        Fiber::suspend();
    }

    $pipeline_data = $pipelines[$pipeline_id];
    $pipeline = $pipeline_data['pipeline'];
    $client = $pipeline_data['client'];

    Fiber::suspend();

    echo "Fiber $fiber_id (Executor $pipeline_id): Executing pipeline $pipeline_id\n";

    // Execute the pipeline - this is where the actual Redis commands are sent
    // Following the Predis pipeline execution pattern
    try {
        $results = $pipeline->execute();
        $pipeline_results[$pipeline_id] = $results;

        echo "Fiber $fiber_id (Executor $pipeline_id): Pipeline $pipeline_id executed with " . count($results) . " results\n";
    } catch (Exception $e) {
        echo "Fiber $fiber_id (Executor $pipeline_id): Error executing pipeline $pipeline_id: " . $e->getMessage() . "\n";
        $pipeline_results[$pipeline_id] = [];
    }

    Fiber::suspend();

    // Clean up the keys created by this pipeline
    if (isset($cleanup_keys[$pipeline_id])) {
        $keys_to_delete = $cleanup_keys[$pipeline_id];
        foreach ($keys_to_delete as $key) {
            $client->del($key);
        }
    }

    Fiber::suspend();
}

function test_pipeline_creation_execution_separated() {
    global $pipeline_results;

    // Create 4 fibers: 2 for pipeline creation, 2 for pipeline execution
    $creator_fibers = [
        'A' => new Fiber(function() {
            return pipeline_creator_fiber(1, 'A');
        }),
        'B' => new Fiber(function() {
            return pipeline_creator_fiber(2, 'B');
        })
    ];

    $executor_fibers = [
        'A' => new Fiber(function() {
            return pipeline_executor_fiber(3, 'A');
        }),
        'B' => new Fiber(function() {
            return pipeline_executor_fiber(4, 'B');
        })
    ];

    // Start all creator fibers first
    foreach ($creator_fibers as $fiber) {
        $fiber->start();
    }

    // Start all executor fibers
    foreach ($executor_fibers as $fiber) {
        $fiber->start();
    }

    // Resume creators to build pipelines
    for ($round = 0; $round < 3; $round++) {
        foreach ($creator_fibers as $fiber) {
            if (!$fiber->isTerminated()) {
                $fiber->resume();
            }
        }
    }

    // Now start interleaved execution between the two executor fibers
    for ($round = 0; $round < 5; $round++) {
        foreach ($executor_fibers as $fiber) {
            if (!$fiber->isTerminated()) {
                $fiber->resume();
            }
        }
    }

    // Final cleanup round - resume any remaining suspended fibers
    $max_cleanup_rounds = 10;
    for ($round = 0; $round < $max_cleanup_rounds; $round++) {
        $all_done = true;

        foreach (array_merge($creator_fibers, $executor_fibers) as $fiber) {
            if (!$fiber->isTerminated()) {
                $fiber->resume();
                $all_done = false;
            }
        }

        if ($all_done) {
            break;
        }
    }

    echo "All pipeline operations completed successfully\n";
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$main_fiber = new Fiber('test_pipeline_creation_execution_separated');
$main_fiber->start();
