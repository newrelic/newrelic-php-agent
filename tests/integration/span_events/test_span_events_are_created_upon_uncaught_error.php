<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that span events are correctly created from any eligible segment, even
when an error is generated and left to the default error handler.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
display_errors=1
log_errors=0
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
    "events_seen": 3
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
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
        "parentId": "??",
        "span.kind": "client",
        "component": "FakeDB"
      },
      {},
      {
        "db.instance": "unknown",
        "peer.hostname": "unknown",
        "peer.address": "unknown:unknown"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/a",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},
      {
        "error.message": "foo",
        "error.class": "E_USER_ERROR"
      }
    ]
  ]
]
*/

/*EXPECT_REGEX
^\s*(PHP )?Fatal error:\s*foo in .*? on line [0-9]+\s*$
*/

function a()
{
    time_nanosleep(0, 100000000);
    trigger_error('foo', E_USER_ERROR);
}

newrelic_record_datastore_segment(
    function () {
        time_nanosleep(0, 100000000);
    }, array(
        'product' => 'FakeDB',
    )
);
a();

echo 'this should never be printed';
