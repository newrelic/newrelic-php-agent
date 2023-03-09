<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Tests newrelic_set_user_attributes() on a nested path with an ucaught exception.
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
      {
        "product": "a_product",
        "account": "a_account",
        "user": "a_user"
      },
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
        "product": "a_product",
        "account": "a_account",
        "user": "a_user"
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
    newrelic_set_user_attributes("b_user", "b_account", "b_product");
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
    newrelic_set_user_attributes("a_user", "a_account", "a_product");
};

a();
b();
