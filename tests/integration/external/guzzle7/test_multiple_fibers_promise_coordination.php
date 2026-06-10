<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that guzzle 7 works correctly when 5 fibers initiate promises and 5 other fibers fulfill them,
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
Fiber 1 initiating promise
Fiber 2 initiating promise
Fiber 3 initiating promise
Fiber 4 initiating promise
Fiber 5 initiating promise
Fiber 6 fulfilling promise 1
Fiber 7 fulfilling promise 2
Fiber 8 fulfilling promise 3
Fiber 9 fulfilling promise 4
Fiber 10 fulfilling promise 5
Promise 1 result: traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
Promise 2 result: traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
Promise 3 result: traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
Promise 4 result: traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
Promise 5 result: traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
All promises completed successfully
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                   [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},            [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [5, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/package/guzzlehttp/guzzle/7/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 6/detected"}, [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/library/Autoloader/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Composer/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure"}, [5, 0, 0, 0, 0, 0]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"}, [5, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, [5, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/Fiber/used"}, ["??", "??", "??", "??", "??", "??"]]
  ]
]
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
        "name": "??",
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
        "http.statusCode": 200
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "??",
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
        "http.statusCode": 200
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "??",
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
        "http.statusCode": 200
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "??",
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
        "http.statusCode": 200
      }
    ],
    [
      {
        "category": "http",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "??",
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
        "http.statusCode": 200
      }
    ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;
use GuzzleHttp\Promise;

// Global storage for promises and results
$promises = [];
$results = [];

function initiating_fiber($fiber_id) {
    global $promises;

    echo "Fiber $fiber_id initiating promise\n";

    /* Create URL. */
    $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

    $client = new Client();

    // Create an async request promise
    $promise = $client->getAsync($url);
    $promises[$fiber_id] = $promise;

    Fiber::suspend();

    return $promise;
}

function fulfilling_fiber($fiber_id, $promise_id) {
    global $promises, $results;

    echo "Fiber $fiber_id fulfilling promise $promise_id\n";

    // Wait for the promise to be available
    while (!isset($promises[$promise_id])) {
        Fiber::suspend();
    }

    $promise = $promises[$promise_id];

    Fiber::suspend();

    // Fulfill the promise by waiting for its result
    // Using Guzzle's recommended promise handling pattern
    try {
        $response = $promise->wait();
        $results[$promise_id] = trim($response->getBody());
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

    // Print results
    for ($i = 1; $i <= 5; $i++) {
        if (isset($results[$i])) {
            echo "Promise $i result: " . $results[$i] . "\n";
        } else {
            echo "Promise $i result: No result available\n";
        }
    }

    echo "All promises completed successfully\n";
}

$main_fiber = new Fiber('test_multiple_fibers_coordination');
$main_fiber->start();
