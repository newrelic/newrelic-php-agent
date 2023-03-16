<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
JIT enabled, should still instrument.
 */

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
zend_extension=opcache.so
error_reporting = E_ALL
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=tracing
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
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "Custom\/main",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "parentId": "??",
        "sampled": true,
        "timestamp": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "??"
      }
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "Custom\/main",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "parentId": "??",
        "sampled": true,
        "timestamp": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "??"
      }
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "Custom\/main",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "parentId": "??",
        "sampled": true,
        "timestamp": "??"
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

/*EXPECT
HelloHelloHello
*/

newrelic_add_custom_tracer('main'); 

function main()
{
  sleep(1);
  echo 'Hello';
}
main();
main();
main();
