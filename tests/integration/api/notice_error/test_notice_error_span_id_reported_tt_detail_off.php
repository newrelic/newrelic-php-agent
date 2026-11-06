<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The spanId of an error event must belong to a span that is reported, otherwise
the error cannot be tied to the span it happened in.
With newrelic.transaction_tracer.detail = 0 the agent drops the segments of
user functions it has no reason to keep. The segment whose ID was given to the
error event must not be one of them. The test reads the span ID of the current
segment with newrelic_get_linking_metadata(), records an error with
newrelic_notice_error(), and then records a Datastore segment in the same
function. The error event's spanId and the Datastore span's parentId are both
expected to equal the span ID read from the linking metadata. That holds on
every PHP version: the segment is the kept function segment with the Observer
API, and the root segment without it.
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT
ok - span id is present in the linking metadata
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "NoticedError",
        "error.message": "msg",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "databaseCallCount": 1,
        "databaseDuration": "??",
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "ENV[SPAN_ID]"
      },
      {},
      {}
    ]
  ]
]
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
        "parentId": "ENV[SPAN_ID]",
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

function notice_error_then_query()
{
    $metadata = newrelic_get_linking_metadata();
    $span_id = isset($metadata['span.id']) ? $metadata['span.id'] : '';

    newrelic_notice_error('msg');

    newrelic_record_datastore_segment(function () {
        time_nanosleep(0, 1000000);
    }, array(
        'product' => 'FakeDB',
    ));

    return $span_id;
}

$span_id = notice_error_then_query();

tap_assert(1 === preg_match('/^[0-9a-f]{16}$/', $span_id), 'span id is present in the linking metadata');

/* Hand the span ID to the expectation blocks above. */
env_var_for_expects('SPAN_ID', $span_id);
