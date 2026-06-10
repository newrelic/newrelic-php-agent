<?php
/*
* Copyright 2020 New Relic Corporation. All rights reserved.
* SPDX-License-Identifier: Apache-2.0
*/

/*DESCRIPTION
The agent SHALL add X-NewRelic-ID and X-NewRelic-Transaction headers to
external calls when the guzzle feature flag is enabled.
*/

/*SKIPIF
 <?php require('skipif.inc');
*/

/*INI
newrelic.distributed_tracing_enabled = true
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
    [{"name":"Supportability/library/Composer/detected"},                               [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/test_concurrent",
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
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/{closure:newrelic\\Guzzle6\\middleware():1}",
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

/*EXPECT_ERROR_EVENTS
null 
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;
use GuzzleHttp\Promise;
use GuzzleHttp\Promise\Utils;
use GuzzleHttp\Exception\RequestException;
use Psr\Http\Message\ResponseInterface;

function test_synchronous()
{
  global $EXTERNAL_HOST;

  $client = new Client(['base_uri' => 'http://' . $EXTERNAL_HOST]);
  $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

  for ($i = 0; $i < 3; $i++) {
    echo $response = $client->get($url)->getBody();
  }
}

function test_concurrent()
{
  global $EXTERNAL_HOST;

  $client = new Client(['base_uri' => 'http://' . $EXTERNAL_HOST]);
  $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');
  $promises = array();

  for ($i = 0; $i < 3; $i++) {
    $promise = $client->getAsync($url);
    $promise->then(
      function (ResponseInterface $r) {
        echo $r->getBody();
      },
      function (RequestException $e) {
        echo $e . "\n";
      }
    );
    $promises[] = $promise;
  }

  Promise\Utils::settle($promises)->wait();
}

//test_synchronous();
test_concurrent();
