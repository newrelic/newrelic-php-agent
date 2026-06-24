<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test adding curl handle while curl multi exec is in flight if fibers are involved.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT
Starting Func 'a'
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
Ending Func 'a'
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
      "name": "Custom\/test_curl_multi_exec_add_handles",
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
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "curl_multi_exec",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_ADD]"
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
      "name": "External\/127.0.0.1\/all",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
      "span.kind": "client",
      "component": "curl"
    },
    {},
    {
      "http.method": "GET",
      "http.url": "http:\/\/ENV[EXTERNAL_HOST]\/cat",
      "http.statusCode": 200
    }
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/a",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ROOT]"
    },
    {},
    {}
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},           [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},[3, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                         [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                               [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/get_linking_metadata"},              ["??", "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/Fiber/used"},                        ["??", "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function test_curl_multi_exec_add_handles()
{
    env_var_for_expects("GUID_TEST_ADD", newrelic_get_linking_metadata()['span.id'] ?? '');
    $url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

    $ch1 = curl_init($url);
    $ch2 = curl_init($url);
    $ch3 = curl_init($url);
    $mh = curl_multi_init();

    curl_multi_add_handle($mh, $ch1);
    curl_multi_add_handle($mh, $ch2);

    $active = 0;
    curl_multi_exec($mh, $active);

    Fiber::suspend();
    curl_multi_add_handle($mh, $ch3);

    Fiber::suspend();
    do {
        curl_multi_exec($mh, $active);
        Fiber::suspend($active);
    } while ($active > 0);

    curl_multi_close($mh);
}

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'a'\n";
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$fiber_a = new Fiber('a');
$fiber_add = new Fiber('test_curl_multi_exec_add_handles');

$fiber_add->start();
$fiber_a->start();
$result = $fiber_add->resume();
do {
  $result = $fiber_add->resume();
} while ($result > 0);
$fiber_a->resume();
$fiber_add->resume();
