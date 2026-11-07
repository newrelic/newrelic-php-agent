<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Every outgoing Guzzle request carries its own traceparent header, and the span
ID in each one must belong to a span that is reported, otherwise the downstream
transaction cannot attach to the trace.
With newrelic.transaction_tracer.detail = 0 the agent drops the segments of
user functions it has no reason to keep. Two requests made from one user
function must each have the segment that handed out the header span ID kept,
so both IDs resolve to reported spans and each is the parent of its External
span.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT
ok - traceparent header was sent for the first request
ok - traceparent header was sent for the second request
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "External\/127.0.0.1\/all",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "ENV[HEADER_SPAN_ID_1]",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.url": "??",
        "http.method": "POST",
        "http.statusCode": 200
      }
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "External\/127.0.0.1\/all",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "ENV[HEADER_SPAN_ID_2]",
        "span.kind": "client",
        "component": "Guzzle 6"
      },
      {},
      {
        "http.url": "??",
        "http.method": "POST",
        "http.statusCode": 200
      }
    ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/unpack_guzzle.php');
require_guzzle(7);

use GuzzleHttp\Client;
use GuzzleHttp\TransferStats;

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

/*
 * Both requests are made from the same user function. The traceparent header
 * is captured as it was handed to the HTTP handler, after the agent's
 * middleware has added it: on_stats receives the request that was actually
 * sent. traceparent is 00-<trace id>-<parent span id>-<flags>.
 */
function make_two_requests($client, $url)
{
    $traceparents = array('', '');

    foreach (array(0, 1) as $i) {
        $client->request('POST', $url, [
            'on_stats' => function (TransferStats $stats) use (&$traceparents, $i) {
                $traceparents[$i] = $stats->getRequest()->getHeaderLine('traceparent');
            },
        ]);
    }

    $span_ids = array();
    foreach ($traceparents as $traceparent) {
        $parts = explode('-', $traceparent);
        $has_span_id = (4 === count($parts)) && (1 === preg_match('/^[0-9a-f]{16}$/', $parts[2]));
        $span_ids[] = $has_span_id ? $parts[2] : '';
    }

    return $span_ids;
}

$client = new Client();
list($span_id_1, $span_id_2) = make_two_requests($client, $url);

tap_assert('' !== $span_id_1, 'traceparent header was sent for the first request');
tap_assert('' !== $span_id_2, 'traceparent header was sent for the second request');

/* Hand the span IDs to the EXPECT_SPAN_EVENTS_LIKE block above. */
env_var_for_expects('HEADER_SPAN_ID_1', $span_id_1);
env_var_for_expects('HEADER_SPAN_ID_2', $span_id_2);
