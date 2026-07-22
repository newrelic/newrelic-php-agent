<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Predis pipelines work correctly when two different fibers have nested pipelines
with multiple calls and the exec execution is interleaved between the two fibers.
FIXED VERSION: Proper fiber interleaving with fluent interface pipelines.
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
Fiber A: Creating outer pipeline
Fiber B: Creating outer pipeline
Fiber A: Adding commands to outer pipeline
Fiber B: Adding commands to outer pipeline
Fiber A: Creating nested pipeline
Fiber B: Creating nested pipeline
Fiber A: Adding commands to nested pipeline
Fiber B: Adding commands to nested pipeline
Fiber A: Executing nested pipeline
Fiber B: Executing nested pipeline
Fiber A: Executing outer pipeline
Fiber B: Executing outer pipeline
Fiber A: Outer pipeline results: 5
Fiber A: Nested pipeline results: 3
Fiber B: Outer pipeline results: 5
Fiber B: Nested pipeline results: 3
All pipeline operations completed successfully
*/

/*EXPECT_METRICS_EXIST
Datastore/Redis/all, 18
Datastore/operation/Redis/del, 2
Datastore/operation/Redis/get, 1
Datastore/operation/Redis/exists, 1
Datastore/operation/Redis/incrby, 1
Datastore/operation/Redis/set, 2
Datastore/operation/Redis/ping, 1
Datastore/operation/Redis/flushdb, 1
Datastore/operation/Redis/lpush, 2
Datastore/operation/Redis/hset, 1
Datastore/operation/Redis/append, 1
Datastore/operation/Redis/strlen, 1
Datastore/operation/Redis/llen, 1
Datastore/operation/Redis/sadd, 1
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "OtherTransaction\/php__FILE__",
      "guid": "??",
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
      "name": "Custom\/pipeline_fiber_a",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
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
      "name": "Datastore\/operation\/Redis\/ping",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_A]",
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
      "parentId": "ENV[GUID_TEST_BASIC_A]",
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
      "parentId": "ENV[GUID_TEST_BASIC_A]",
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
      "category": "generic",
      "type": "Span",
      "guid": "ENV[GUID_TEST_BASIC_B]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/pipeline_fiber_b",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
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
      "name": "Datastore\/operation\/Redis\/sadd",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_B]",
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
      "name": "Datastore\/operation\/Redis\/llen",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_B]",
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
      "name": "Datastore\/operation\/Redis\/strlen",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_B]",
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
      "name": "Datastore\/operation\/Redis\/append",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_B]",
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
      "name": "Datastore\/operation\/Redis\/hset",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_B]",
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
      "name": "Datastore\/operation\/Redis\/lpush",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC_B]",
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

// Global storage for pipeline results
$pipeline_results = [];

function pipeline_fiber_a($fiber_id) {
    global $REDIS_HOST, $REDIS_PORT, $pipeline_results;

    env_var_for_expects("GUID_TEST_BASIC_A", newrelic_get_linking_metadata()['span.id'] ?? '');


    $client = new Predis\Client(array(
        'host' => $REDIS_HOST,
        'port' => $REDIS_PORT,
    ));

    try {
        $client->connect();
    } catch (Exception $e) {
        die("skip: " . $e->getMessage() . "\n");
    }

    $key_prefix = "fiber_a";
    $main_key = $key_prefix . "_main";
    $nested_key = $key_prefix . "_nested";

    echo "Fiber A: Creating outer pipeline\n";

    // Create outer pipeline using fluent interface
    $outer_pipeline = $client->pipeline();

    Fiber::suspend();

    echo "Fiber A: Adding commands to outer pipeline\n";

    // Add commands to outer pipeline
    $outer_pipeline->ping();
    $outer_pipeline->flushdb();
    $outer_pipeline->set($main_key, 'main_value');
    $outer_pipeline->exists($main_key);

    Fiber::suspend();

    echo "Fiber A: Creating nested pipeline\n";

    // Create nested pipeline using fluent interface - fixed approach
    $nested_pipeline = $client->pipeline();

    Fiber::suspend();

    echo "Fiber A: Adding commands to nested pipeline\n";

    // Add commands to nested pipeline
    $nested_pipeline->set($nested_key, 'nested_value');
    $nested_pipeline->get($nested_key);
    $nested_pipeline->incrby($nested_key . '_counter', 5);

    Fiber::suspend();

    echo "Fiber A: Executing nested pipeline\n";

    // Execute nested pipeline at the proper suspension point
    $nested_results = $nested_pipeline->execute();

    Fiber::suspend();

    // Continue with outer pipeline
    $outer_pipeline->mget($main_key, $nested_key);

    echo "Fiber A: Executing outer pipeline\n";

    // Execute outer pipeline
    $outer_results = $outer_pipeline->execute();

    Fiber::suspend();

    // Store results for verification
    $pipeline_results[$fiber_id] = [
        'outer' => $outer_results,
        'nested' => $nested_results
    ];

    echo "Fiber A: Outer pipeline results: " . count($outer_results) . "\n";
    echo "Fiber A: Nested pipeline results: " . count($nested_results) . "\n";

    // Clean up
    $client->del($main_key, $nested_key, $nested_key . '_counter');

    Fiber::suspend();
}

function pipeline_fiber_b($fiber_id) {
    global $REDIS_HOST, $REDIS_PORT, $pipeline_results;

    env_var_for_expects("GUID_TEST_BASIC_B", newrelic_get_linking_metadata()['span.id'] ?? '');

    $client = new Predis\Client(array(
        'host' => $REDIS_HOST,
        'port' => $REDIS_PORT,
    ));

    try {
        $client->connect();
    } catch (Exception $e) {
        die("skip: " . $e->getMessage() . "\n");
    }

    $key_prefix = "fiber_b";
    $main_key = $key_prefix . "_main";
    $nested_key = $key_prefix . "_nested";
    $hash_key = $key_prefix . "_hash";
    $list_key = $key_prefix . "_list";
    $set_key = $key_prefix . "_set";

    echo "Fiber B: Creating outer pipeline\n";

    // Create outer pipeline using fluent interface - fixed approach
    $outer_pipeline = $client->pipeline();

    Fiber::suspend();

    echo "Fiber B: Adding commands to outer pipeline\n";

    // Use different operations than fiber A
    $outer_pipeline->lpush($list_key, 'list_value_b');  // Instead of ping()
    $outer_pipeline->hset($hash_key, 'field', 'hash_value_b');  // Instead of flushdb()
    $outer_pipeline->append($main_key, 'appended_b');  // Instead of set()
    $outer_pipeline->strlen($main_key);  // Instead of exists()

    Fiber::suspend();

    echo "Fiber B: Creating nested pipeline\n";

    // Create nested pipeline using fluent interface
    $nested_pipeline = $client->pipeline();

    Fiber::suspend();

    echo "Fiber B: Adding commands to nested pipeline\n";

    // Use different operations in nested pipeline
    $nested_pipeline->lpush($nested_key, 'nested_list_b');  // Instead of set()
    $nested_pipeline->llen($nested_key);  // Instead of get()
    $nested_pipeline->sadd($set_key, 'set_member_b');  // Instead of incrby()

    Fiber::suspend();

    echo "Fiber B: Executing nested pipeline\n";

    // Execute nested pipeline - this now happens at the proper suspension point
    $nested_results = $nested_pipeline->execute();

    Fiber::suspend();

    // Continue with outer pipeline - use hget instead of mget
    $outer_pipeline->hget($hash_key, 'field');

    echo "Fiber B: Executing outer pipeline\n";

    // Execute outer pipeline
    $outer_results = $outer_pipeline->execute();

    Fiber::suspend();

    // Store results for verification
    $pipeline_results[$fiber_id] = [
        'outer' => $outer_results,
        'nested' => $nested_results
    ];

    echo "Fiber B: Outer pipeline results: " . count($outer_results) . "\n";
    echo "Fiber B: Nested pipeline results: " . count($nested_results) . "\n";

    // Clean up
    $client->del($main_key, $nested_key, $nested_key . '_counter', $hash_key, $list_key, $set_key);

    Fiber::suspend();
}

function test_nested_pipeline_interleaved() {
    global $pipeline_results;

    // Create two fibers for interleaved pipeline execution
    $fiber_a = new Fiber(function() {
        return pipeline_fiber_a('fiber_a');
    });

    $fiber_b = new Fiber(function() {
        return pipeline_fiber_b('fiber_b');
    });

    // Start both fibers
    $fiber_a->start();
    $fiber_b->start();

    // Interleave execution between the two fibers
    for ($round = 0; $round < 8; $round++) {
        if (!$fiber_a->isTerminated()) {
            $fiber_a->resume();
        }

        if (!$fiber_b->isTerminated()) {
            $fiber_b->resume();
        }
    }

    // Final cleanup round
    while (!$fiber_a->isTerminated() || !$fiber_b->isTerminated()) {
        if (!$fiber_a->isTerminated()) {
            $fiber_a->resume();
        }

        if (!$fiber_b->isTerminated()) {
            $fiber_b->resume();
        }
    }

    echo "All pipeline operations completed successfully\n";
}

$main_fiber = new Fiber('test_nested_pipeline_interleaved');
$main_fiber->start();
