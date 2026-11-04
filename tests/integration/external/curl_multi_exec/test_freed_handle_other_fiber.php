<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A curl multi handle that was freed in one fiber is cleaned up the next time
curl metadata is read, which can happen in another fiber. Ending its
curl_multi_exec segment SHALL NOT change the span that is current in that other
fiber: a span parameter added there is set on the span of that fiber.
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
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT
Hello world!
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "ENV[GUID_ONE]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_one",
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
      "name": "curl_multi_exec",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ONE]"
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
      "http.url": "??",
      "http.statusCode": 200
    }
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "ENV[GUID_TWO]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_two",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {"after_drain": 1},
    {}
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function fiber_one()
{
    global $EXTERNAL_HOST;

    env_var_for_expects("GUID_ONE", newrelic_get_linking_metadata()['span.id'] ?? '');

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $EXTERNAL_HOST);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);

    $mh = curl_multi_init();
    curl_multi_add_handle($mh, $ch);

    $active = 0;

    do {
        curl_multi_exec($mh, $active);
        Fiber::suspend($active);
    } while ($active > 0);

    echo curl_multi_getcontent($ch) . "\n";
    curl_multi_remove_handle($mh, $ch);
    /* The multi handle is freed when the function returns. */
}

function fiber_two()
{
    env_var_for_expects("GUID_TWO", newrelic_get_linking_metadata()['span.id'] ?? '');

    /* Reading curl metadata cleans up the handles that were freed. */
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'GET');

    newrelic_add_custom_span_parameter('after_drain', 1);
}

$one = new Fiber('fiber_one');
$one->start();
while (!$one->isTerminated()) {
    $one->resume();
}

$two = new Fiber('fiber_two');
$two->start();
