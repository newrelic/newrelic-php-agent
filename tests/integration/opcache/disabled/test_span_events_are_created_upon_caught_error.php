<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that span events are correctly created from any eligible segment, even
when an error is generated and handled by user error handler. The span that
generated the error should have error attributes. Additionally error events
should be created.
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
display_errors=1
log_errors=0
error_reporting = E_ALL
opcache.enable=0
opcache.enable_cli=0
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=function
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 100,
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "E_USER_ERROR",
        "error.message": "foo",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
        "databaseDuration": "??",
        "databaseCallCount": 1,
        "nr.transactionGuid": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "spanId": "??"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 4
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
        "error.class": "E_USER_ERROR",
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "??"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/{closure}",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "??"
      }
    ]
  ]
]
*/

/*EXPECT_REGEX
^\s*(PHP )?Fatal error:\s*foo in .*? on line [0-9]+\s*$
*/
require('opcache_test.inc');

set_error_handler(
    function (int $errno, string $errstr) {
        time_nanosleep(0, 100000000);
        return false;
    }
);

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
