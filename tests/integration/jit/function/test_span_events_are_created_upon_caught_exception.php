<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that span events are correctly created from any eligible segment when a caught exception occurs.
Putting in a try/catch block means an exception is NOT handled by the exception handler.
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
error_reporting = E_ALL
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=function
*/

/*PHPMODULES
zend_extension=opcache.so
*/

/*EXPECT_ERROR_EVENTS
null
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
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
              "code.lineno": "??",
              "code.filepath": "__FILE__",
              "code.function": "a"
      }
    ]
  ]
]
*/

/*EXPECT
*/

set_exception_handler(
    function () {
        time_nanosleep(0, 100000000);
        echo 'this should never be printed';
        exit(0);
    }
);

function a()
{
    time_nanosleep(0, 100000000);
    throw new RuntimeException('oops');
}

newrelic_record_datastore_segment(
    function () {
        time_nanosleep(0, 100000000);
    }, array(
        'product' => 'FakeDB',
    )
);

try {
a();
} catch (RuntimeException $e) {
}