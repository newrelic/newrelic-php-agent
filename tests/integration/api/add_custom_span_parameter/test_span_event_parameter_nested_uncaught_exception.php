<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Tests newrelic_add_custom_span_parameter() on a nested path that includes an uncaught exception.
The custom span parameters for `a` should not show up on the span error event associated with for `b`.
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
        "error.class": "RuntimeException",
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
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
    "events_seen": 6
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
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
        "error.class": "RuntimeException"
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
      {
        "double": 1.50000,
        "bool": false,
        "int": 7,
        "string": "a_str"
      },
      {
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
        "name": "Custom\/b",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},      
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
        "error.class": "RuntimeException",
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
        "name": "Custom\/c",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {},
      {
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
        "error.class": "RuntimeException",
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
        "name": "Custom\/fraction",
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
        "error.message": "Uncaught exception 'RuntimeException' with message 'Division by zero' in __FILE__:??",
        "error.class": "RuntimeException",
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "??"
      }
    ]
  ]
]
*/


/*EXPECT_REGEX
.*(PHP )?Fatal error: Uncaught.*RuntimeException.*zero.*
*/

function c()
{
    time_nanosleep(0, 100000000);
    echo fraction(5) . "\n";
    echo fraction(0) . "\n";
}

function b()
{
    time_nanosleep(0, 100000000);
    c();
    newrelic_add_custom_span_parameter("string", "b_str");
    newrelic_add_custom_span_parameter("int", 7);
    newrelic_add_custom_span_parameter("bool", false);
    newrelic_add_custom_span_parameter("double", 1.5);
}

function fraction($x) {
    time_nanosleep(0, 100000000);
    if (!$x) {
        throw new RuntimeException('Division by zero');
    }
    return 1/$x;
}

function a()
{
    time_nanosleep(0, 100000000);
    echo "Hello\n";
    newrelic_add_custom_span_parameter("string", "a_str");
    newrelic_add_custom_span_parameter("int", 7);
    newrelic_add_custom_span_parameter("bool", false);
    newrelic_add_custom_span_parameter("double", 1.5);
};

a();
b();
