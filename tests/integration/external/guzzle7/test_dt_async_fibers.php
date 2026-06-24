<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that guzzle 7 distributed tracing works correctly with async requests when using Fibers.
The agent SHALL add X-NewRelic-ID and X-NewRelic-Transaction headers to external calls
when the guzzle feature flag is enabled and fibers are involved.
*/

/*SKIPIF
<?php
require('skipif.inc');
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
?>
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.fibers.disabled = false
*/

/*EXPECT
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                                           [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                                                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                                          [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},                  [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},                             [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/package/guzzlehttp/guzzle/7/detected"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 6/detected"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure"},  ["??", "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},                          [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                             [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Autoloader/detected"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Composer/detected"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/Fiber/used"},                                          ["??", "??", "??", "??", "??", "??"]]
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
    ]
]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;
use GuzzleHttp\Promise;
use GuzzleHttp\Promise\Utils;
use GuzzleHttp\Exception\RequestException;
use Psr\Http\Message\ResponseInterface;

// Global storage for promises and responses
$promises = [];
$responses = [];

function async_request_fiber($fiber_id, $client, $url) {
    global $promises, $responses;

    // Create async request
    $promise = $client->getAsync($url);

    // Add promise callbacks (NO fiber suspension in callbacks)
    $promise->then(
        function (ResponseInterface $r) use ($fiber_id) {
            global $responses;
            $responses[$fiber_id] = $r->getBody();
            return $r;
        },
        function (RequestException $e) use ($fiber_id) {
            global $responses;
            $responses[$fiber_id] = $e->getMessage();
            throw $e;
        }
    );

    // Store promise for later resolution
    $promises[$fiber_id] = $promise;

    // Suspend the fiber after creating the promise
    // This allows other fibers to run while this one waits
    Fiber::suspend();

    return $promise;
}

function test_concurrent_fibers()
{
    global $EXTERNAL_HOST, $promises, $responses;

    $client = new Client(['base_uri' => 'http://' . $EXTERNAL_HOST]);
    $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

    // Create fibers for async requests
    $request_fibers = [];
    for ($i = 0; $i < 3; $i++) {
        $request_fibers[$i] = new Fiber(function() use ($i, $client, $url) {
            return async_request_fiber($i, $client, $url);
        });
    }

    // Start all request fibers
    foreach ($request_fibers as $fiber) {
        $fiber->start();
    }

    // Resume fibers to create promises
    foreach ($request_fibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    // Resume fibers to let them suspend after creating promises
    foreach ($request_fibers as $fiber) {
        if (!$fiber->isTerminated()) {
            $fiber->resume();
        }
    }

    // Wait for promises to settle using Guzzle's Utils::settle()
    // This follows the original test pattern from the original test
    if (!empty($promises)) {
        $settled = Promise\Utils::settle($promises)->wait();

        // Process settled promises and output results
        foreach ($settled as $index => $result) {
            if ($result['state'] === 'fulfilled') {
                echo $responses[$index];
            } else {
                echo "Request $index failed\n";
            }
        }
    }
}

function test_main_fiber() {
    // Run the concurrent fiber test
    test_concurrent_fibers();
}

$main_fiber = new Fiber('test_main_fiber');
$main_fiber->start();
