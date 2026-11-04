<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Fibers that create a curl multi handle, run it and free it SHALL each get a
curl_multi_exec segment of their own, parented to the span of the fiber, also
when PHP gives the id of a freed handle to the handle of the next fiber. A span
parameter added after the transfers is set on the span of the fiber.
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
Hello world!
Hello world!
Hello world!
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "ENV[GUID_FIBER_1]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_func",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {"done": 1},
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
      "parentId": "ENV[GUID_FIBER_1]"
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
      "guid": "ENV[GUID_FIBER_2]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_func",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {"done": 2},
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
      "parentId": "ENV[GUID_FIBER_2]"
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
      "guid": "ENV[GUID_FIBER_3]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_func",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {"done": 3},
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
      "parentId": "ENV[GUID_FIBER_3]"
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
      "guid": "ENV[GUID_FIBER_4]",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/fiber_func",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {"done": 4},
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
      "parentId": "ENV[GUID_FIBER_4]"
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
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function fiber_func($n)
{
    global $EXTERNAL_HOST;

    env_var_for_expects("GUID_FIBER_$n", newrelic_get_linking_metadata()['span.id'] ?? '');

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

    /* The handles are freed when the function returns. */
    newrelic_add_custom_span_parameter('done', $n);
}

for ($n = 1; $n <= 4; $n++) {
    $fiber = new Fiber('fiber_func');
    $fiber->start($n);
    while (!$fiber->isTerminated()) {
        $fiber->resume();
    }
}
