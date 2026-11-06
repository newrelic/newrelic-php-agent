<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The span ID in the outgoing traceparent header of a Guzzle request must belong
to a span that is reported, otherwise the downstream transaction cannot attach
to the trace and is shown as having a missing parent.
With newrelic.transaction_tracer.detail = 0 the agent drops the segments of
user functions it has no reason to keep. The ID in the header comes from one of
those segments, so it is never reported. This test fails until that is fixed
(https://github.com/newrelic/newrelic-php-agent/issues/1370).
The span with the ID sent in the header is expected to be the parent of the
External span. That holds when the segment the ID was taken from is kept (or
when the ID is taken from the nearest segment that is kept). A fix that sends
the ID of the External span itself would need this expectation changed to
compare the External span's guid instead.
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
ok - traceparent header was sent
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
        "parentId": "ENV[HEADER_SPAN_ID]",
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
require_guzzle(6);

use GuzzleHttp\Client;
use GuzzleHttp\TransferStats;

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) .  '/../../../include/tracing_endpoint.php');

/*
 * Capture the traceparent header as it was handed to the HTTP handler, after
 * the agent's middleware has added it. on_stats receives the request that was
 * actually sent.
 */
$traceparent = '';
$client = new Client();
$client->request('POST', $url, [
    'on_stats' => function (TransferStats $stats) use (&$traceparent) {
        $traceparent = $stats->getRequest()->getHeaderLine('traceparent');
    },
]);

/* traceparent is 00-<trace id>-<parent span id>-<flags>. */
$parts = explode('-', $traceparent);
$has_span_id = (4 === count($parts)) && (1 === preg_match('/^[0-9a-f]{16}$/', $parts[2]));

tap_assert($has_span_id, 'traceparent header was sent');

/* Hand the span ID to the EXPECT_SPAN_EVENTS_LIKE block above. */
env_var_for_expects('HEADER_SPAN_ID', $has_span_id ? $parts[2] : '');
