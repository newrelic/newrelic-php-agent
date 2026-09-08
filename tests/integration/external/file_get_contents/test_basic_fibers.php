<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report External metrics for file_get_contents() requests if fibers are involved.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.fibers.disabled = false
*/

/*EXPECT_REGEX
Hello world!Starting Func 'a'
Hello world!Hello world!Ending Func 'a'
Hello world!Hello
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
      "name": "Custom\/test_file_get_contents",
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
      "parentId": "ENV[GUID_TEST_FGC]",
      "span.kind": "client",
      "component": "file_get_contents"
    },
    {},
    {
      "http.method": "GET",
      "http.url": "http:\/\/ENV[EXTERNAL_HOST]",
      "http.statusCode": 0
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
    [{"name":"Supportability/TraceContext/Create/Success"},           [5, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},[5, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                         [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                    [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                               [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                        [5, "??", "??", "??", "??", "??"]],
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

function test_file_get_contents()
{
    global $EXTERNAL_HOST;

    env_var_for_expects("GUID_TEST_FGC", newrelic_get_linking_metadata()['span.id'] ?? '');
    $url = 'http://' . $EXTERNAL_HOST;


    // only URL
    echo file_get_contents ($url);

    // no context
    Fiber::suspend();
    echo file_get_contents ($url, false);

    // NULL context
    echo file_get_contents ($url, false, NULL);

    // NULL context with offset and maxlen
    Fiber::suspend();
    echo file_get_contents ($url, false, NULL, 0, 50000);

    // small maxlen
    echo file_get_contents ($url, false, NULL, 0, 5);
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
$fiber_fgc = new Fiber('test_file_get_contents');

$fiber_fgc->start();
$fiber_a->start();
$fiber_fgc->resume();
$fiber_a->resume();
$fiber_fgc->resume();
