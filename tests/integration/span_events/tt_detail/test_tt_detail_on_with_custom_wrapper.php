<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are enabled, test that calls to any long enough functions appear as spans in span events regardless if they are wrapped or not.
*/

/*INI
newrelic.transaction_tracer.detail = 1
newrelic.special.expensive_node_min = 50us
*/

/*EXPECT
my_custom_function called
my_custom_function called
No alarms and no surprises.
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
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction/php__FILE__"
      },
      {},
      {}
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/my_custom_function",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "my_custom_function"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/my_custom_function",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": "??",
        "code.filepath": "__FILE__",
        "code.function": "my_custom_function"
      }
    ]
  ]
]
*/

function my_custom_function() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  time_nanosleep(0, 100 * 1000); // 100 microseconds should be enough (= 2 x newrelic.special.expensive_node_min)
  echo 'my_custom_function called' . PHP_EOL;
}

// Even though this function is not wrapped yet, this call will be a span in span events
// because tt details are enabled and it takes long enough to execute it.
my_custom_function();

// Add a custom wrapper to ensure that all future calls will be recorded as spans in span events
newrelic_add_custom_tracer('my_custom_function');

// This call will be a span in span events because it is wrapped
my_custom_function();

echo 'No alarms and no surprises.'  . PHP_EOL;
