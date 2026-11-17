<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The span ID in the traceparent header added by
newrelic_insert_distributed_trace_headers() must belong to a span that is
reported, otherwise the downstream transaction cannot attach to the trace.
With newrelic.transaction_tracer.detail = 0 the agent drops the segments of
user functions it has no reason to keep. The segment whose ID went into the
header must not be one of them. The Datastore segment recorded afterwards in
the same function is a child of that segment, so its span's parent ID has to
equal the span ID from the header. That holds on every PHP version: the parent
is the kept function segment with the Observer API, and the root segment
without it.
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT
ok - insert function succeeded
ok - traceparent header is present
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Datastore\/statement\/FakeDB\/other\/other",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "datastore",
        "parentId": "ENV[HEADER_SPAN_ID]",
        "span.kind": "client",
        "component": "FakeDB"
      },
      {},
      {
        "db.instance": "unknown",
        "peer.hostname": "unknown",
        "peer.address": "unknown:unknown"
      }
    ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

function insert_headers_then_query()
{
    $headers = array();
    $inserted = newrelic_insert_distributed_trace_headers($headers);

    newrelic_record_datastore_segment(function () {
        time_nanosleep(0, 1000000);
    }, array(
        'product' => 'FakeDB',
    ));

    return array($inserted, $headers);
}

list($inserted, $headers) = insert_headers_then_query();

/* traceparent is 00-<trace id>-<parent span id>-<flags>. */
$traceparent = isset($headers['traceparent']) ? $headers['traceparent'] : '';
$parts = explode('-', $traceparent);
$has_span_id = (4 === count($parts)) && (1 === preg_match('/^[0-9a-f]{16}$/', $parts[2]));

tap_assert($inserted, 'insert function succeeded');
tap_assert($has_span_id, 'traceparent header is present');

/* Hand the span ID to the EXPECT_SPAN_EVENTS_LIKE block above. */
env_var_for_expects('HEADER_SPAN_ID', $has_span_id ? $parts[2] : '');
