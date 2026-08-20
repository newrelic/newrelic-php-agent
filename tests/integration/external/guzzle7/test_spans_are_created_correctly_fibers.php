<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that spans are created correctly for guzzle 7 external requests when using Fibers.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
?>
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 1
newrelic.code_level_metrics.enabled = 0
newrelic.fibers.disabled = false
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "OtherTransaction/php__FILE__",
      "guid": "??",
      "type": "Span",
      "category": "generic",
      "priority": "??",
      "sampled": true,
      "nr.entryPoint": true,
      "timestamp": "??",
      "transaction.name": "OtherTransaction/php__FILE__"
    },
    {},
    {}
  ],
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "External/127.0.0.1/all",
      "guid": "??",
      "type": "Span",
      "category": "http",
      "priority": "??",
      "sampled": true,
      "timestamp": "??",
      "parentId": "??",
      "span.kind": "client",
      "component": "Guzzle 6"
    },
    {},
    {
        "http.url": "??",
        "http.method": "GET",
        "http.statusCode": 200
    }
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"External/127.0.0.1/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/package/guzzlehttp/guzzle/7/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 6/detected"}, [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/library/Autoloader/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Composer/detected"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure"}, [1, 0, 0, 0, 0, 0]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/Fiber/used"}, ["??", "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_guzzle(7);

use GuzzleHttp\Client;

function test_spans() {
    /* Create URL. */
    $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

    $client = new Client();

    Fiber::suspend();

    $response = $client->get($url);
    echo $response->getBody();
}

$fiber = new Fiber('test_spans');
$fiber->start();
$fiber->resume();
