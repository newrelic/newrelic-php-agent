<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that a caught exception is correctly handled when a notice error is also
called.
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
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
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
        "error.class": "Exception",
        "error.message": "Noticed exception 'Exception' with message 'Notice me' in __FILE__:??",
        "transactionName": "OtherTransaction\/php__FILE__",
        "duration": "??",
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
        "name": "Custom\/a",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 152,
        "code.filepath": "__FILE__",
        "code.function": "a"
      }
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/fraction",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},
      {
        "error.message": "Noticed exception 'Exception' with message 'Notice me' in __FILE__:??",
        "error.class": "Exception",
        "code.lineno": 136,
        "code.filepath": "__FILE__",
        "code.function": "fraction"
      }
    ]
  ]
]
*/

/*EXPECT
Hello
Caught exception: Division by zero
*/


function fraction()
{
    time_nanosleep(0, 100000000);
    try {
        $x = 0;
        if (!$x) {
          throw new RuntimeException('Division by zero');
    }
        echo 1/$x . "\n";
    
    } catch (RuntimeException $e) {
        echo("Caught exception: " . $e->getMessage() . "\n");
    }
    newrelic_notice_error(new Exception('Notice me'));
}

function a()
{
    time_nanosleep(0, 100000000);
    echo "Hello\n";
};

a();
fraction();
