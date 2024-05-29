<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Span events must be sent when distributed tracing and span events are enabled.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 2
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
        "code.lineno": 80,
        "code.filepath": "__FILE__",
        "code.function": "main"
      }
    ]
  ]
]
*/

/*EXPECT
Hello
*/

if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
}
newrelic_add_custom_tracer('main');
function main()
{
  echo 'Hello';
}
main();

