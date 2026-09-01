<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report External metrics for curl_exec() requests if fibers are involved.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
if (version_compare(PHP_VERSION, "8.5", ">=")) {
  die("skip: PHP >= 8.5.0 curl_close deprecated\n");
}
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT
ok - simple hostname
Starting Func 'a'
ok - strip query string
ok - strip fragment
Ending Func 'a'
ok - strip credentials
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
      "name": "Custom\/test_curl",
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
      "parentId": "ENV[GUID_TEST_CURL]",
      "span.kind": "client",
      "component": "curl"
    },
    {},
    {
      "http.method": "GET",
      "http.url": "http:\/\/ENV[EXTERNAL_HOST]\/",
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
    [{"name":"Supportability/TraceContext/Create/Success"},           [4, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},[4, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                         [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                    [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                               [4, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
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

function test_curl()
{
    global $EXTERNAL_HOST;

    env_var_for_expects("GUID_TEST_CURL", newrelic_get_linking_metadata()['span.id'] ?? '');
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

    curl_setopt($ch, CURLOPT_URL, 'http://' . $EXTERNAL_HOST . '');
    tap_not_equal(false, curl_exec($ch), 'simple hostname');

    /* Query string should be stripped. */
    Fiber::suspend();
    curl_setopt($ch, CURLOPT_URL, 'http://' . $EXTERNAL_HOST . '?a=1&b=2');
    tap_not_equal(false, curl_exec($ch), 'strip query string');

    /* Fragment should be stripped. */
    curl_setopt($ch, CURLOPT_URL, 'http://' . $EXTERNAL_HOST . '/#fragment');
    tap_not_equal(false, curl_exec($ch), 'strip fragment');

    /* Auth credentials should be stripped. */
    Fiber::suspend();
    curl_setopt($ch, CURLOPT_URL, 'http://user:pass@' . $EXTERNAL_HOST . '');
    tap_not_equal(false, curl_exec($ch), 'strip credentials');

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
$fiber_curl = new Fiber('test_curl');

$fiber_curl->start();
$fiber_a->start();
$fiber_curl->resume();
$fiber_a->resume();
$fiber_curl->resume();
