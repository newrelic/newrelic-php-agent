<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that guzzle 7 works correctly when 5 fibers initiate 3 promises each and 5 other fibers fulfill them,
demonstrating complex fiber coordination with HTTP requests.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
require("skipif.inc");
?>
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT
Fiber 1 initiating promises
Fiber 2 initiating promises
Fiber 3 initiating promises
Fiber 4 initiating promises
Fiber 5 initiating promises
Fiber 6 fulfilling promises 1
Fiber 7 fulfilling promises 2
Fiber 8 fulfilling promises 3
Fiber 9 fulfilling promises 4
Fiber 10 fulfilling promises 5
All promises completed successfully
Fiber#0 child 0 name: External/url1_1/all
ok - This External segment has correct Guzzle context.
Fiber#0 child 1 name: External/url1_2/all
ok - This External segment has correct Guzzle context.
Fiber#0 child 2 name: External/url1_3/all
ok - This External segment has correct Guzzle context.
Fiber#1 child 0 name: External/url2_1/all
ok - This External segment has correct Guzzle context.
Fiber#1 child 1 name: External/url2_2/all
ok - This External segment has correct Guzzle context.
Fiber#1 child 2 name: External/url2_3/all
ok - This External segment has correct Guzzle context.
Fiber#2 child 0 name: External/url3_1/all
ok - This External segment has correct Guzzle context.
Fiber#2 child 1 name: External/url3_2/all
ok - This External segment has correct Guzzle context.
Fiber#2 child 2 name: External/url3_3/all
ok - This External segment has correct Guzzle context.
Fiber#3 child 0 name: External/url4_1/all
ok - This External segment has correct Guzzle context.
Fiber#3 child 1 name: External/url4_2/all
ok - This External segment has correct Guzzle context.
Fiber#3 child 2 name: External/url4_3/all
ok - This External segment has correct Guzzle context.
Fiber#4 child 0 name: External/url5_1/all
ok - This External segment has correct Guzzle context.
Fiber#4 child 1 name: External/url5_2/all
ok - This External segment has correct Guzzle context.
Fiber#4 child 2 name: External/url5_3/all
ok - This External segment has correct Guzzle context.
*/

/*EXPECT_METRICS_EXIST
External/url5_1/all
External/url5_2/all
External/url5_3/all
External/url4_1/all
External/url4_2/all
External/url4_3/all
External/url3_1/all
External/url3_2/all
External/url3_3/all
External/url2_1/all
External/url2_2/all
External/url2_3/all
External/url1_1/all
External/url1_2/all
External/url1_3/all
Supportability/TraceContext/Create/Success, 15
Supportability/DistributedTrace/CreatePayload/Success, 15
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url1_1\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url1_2\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url1_3\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url2_1\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url2_2\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url2_3\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url3_1\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url3_2\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url3_3\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url4_1\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url4_2\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url4_3\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url5_1\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url5_2\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "External\/url5_3\/all",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.method": "GET",
        "http.url": "??",
        "http.statusCode": 0
      }
    ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/integration.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

require_guzzle(7);

use NewRelic\Integration\Transaction;

use GuzzleHttp\Client;
use GuzzleHttp\Promise;

// Global storage for promises and results
$promises = [];
$results = [];

function initiating_fiber($fiber_id) {
    global $promises;

    echo "Fiber $fiber_id initiating promises\n";

    $client = new Client();
    $fiber_promises = [];

    // Create 3 async request promises
    for ($i = 1; $i <= 3; $i++) {
        $url = "http://url" . $fiber_id . "_" . $i;
        $promise = $client->getAsync($url);
        $fiber_promises[] = $promise;
    }

    $promises[$fiber_id] = $fiber_promises;

    Fiber::suspend();

    return $fiber_promises;
}

function fulfilling_fiber($fiber_id, $promise_id) {
    global $promises, $results;

    echo "Fiber $fiber_id fulfilling promises $promise_id\n";

    // Wait for the promises to be available
    while (!isset($promises[$promise_id])) {
        Fiber::suspend();
    }

    $fiber_promises = $promises[$promise_id];

    Fiber::suspend();

    // Fulfill all promises by waiting for their results
    // Using Guzzle's recommended promise handling pattern
    try {
        // Wait for all promises to complete and collect results
        $responses = [];
        foreach ($fiber_promises as $promise) {
            $response = $promise->wait();
            $responses[] = trim($response->getBody());
        }
        // Use the first response for display (all should be similar)
        $results[$promise_id] = $responses[0];
    } catch (Exception $e) {
        $results[$promise_id] = "Error: " . $e->getMessage();
    }

    Fiber::suspend();
}

function test_multiple_fibers_coordination() {
    global $results, $promises;

    // Create 5 initiating fibers
    $initiating_fibers = [];
    for ($i = 1; $i <= 5; $i++) {
        $initiating_fibers[$i] = new Fiber(function() use ($i) {
            return initiating_fiber($i);
        });
    }

    // Create 5 fulfilling fibers
    $fulfilling_fibers = [];
    for ($i = 6; $i <= 10; $i++) {
        $promise_id = $i - 5; // Maps fiber 6->promise 1, fiber 7->promise 2, etc.
        $fulfilling_fibers[$i] = new Fiber(function() use ($i, $promise_id) {
            return fulfilling_fiber($i, $promise_id);
        });
    }

    // Start all initiating fibers
    foreach ($initiating_fibers as $fiber) {
        $fiber->start();
    }

    // Start all fulfilling fibers
    foreach ($fulfilling_fibers as $fiber) {
        $fiber->start();
    }

    // Resume initiating fibers to let them create promises
    foreach ($initiating_fibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    // Resume fulfilling fibers multiple times to let them process
    for ($round = 0; $round < 5; $round++) {
        foreach ($fulfilling_fibers as $fiber) {
            if (!$fiber->isTerminated()) {
                $fiber->resume();
            }
        }

        // Give initiating fibers a chance if they're still running
        foreach ($initiating_fibers as $fiber) {
            if (!$fiber->isTerminated()) {
                $fiber->resume();
            }
        }
    }

    // Wait a bit for any remaining async operations to complete
    usleep(100000); // 100ms

    // Final cleanup round
    foreach ($fulfilling_fibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    echo "All promises completed successfully\n";
}

$main_fiber = new Fiber('test_multiple_fibers_coordination');
$main_fiber->start();


new Transaction;

$txn = new Transaction;
$middleware_segments = $txn->getTrace()->findSegmentsBySubstring('newrelic\\Guzzle6\\middleware');
$initiating_fiber_segments = $txn->getTrace()->findSegmentsByName('Custom/initiating_fiber');

$fiber_index = 0;
foreach ($initiating_fiber_segments as $fiber_segment) {
    $fiber_segment_context = $fiber_segment->attributes->async_context;
    $child_index = 0;
    foreach ($middleware_segments as $parent_segment) {
      $parent_context = $parent_segment->attributes->async_context;
      if ($parent_context === $fiber_segment_context) {
          $children = $parent_segment->children;
          foreach ($children as $child) {
              if (isset($child->attributes->uri)) {
                  echo "Fiber#$fiber_index child $child_index name: " . $child->name . "\n";
                  $child_index++;
                  $child_context = $child->attributes->async_context;
                  tap_equal(true, str_contains($child_context, "Guzzle"), 'This External segment has correct Guzzle context.');
              }
          }
      }
    }
    $fiber_index++;
}
