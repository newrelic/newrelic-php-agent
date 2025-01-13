<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are disabled, test that only calls to custom wrapped functions appear as spans in span events after they have been wrapped using API.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 50us
*/

/*EXPECT
function_exceeding_tt_detail_threshold called
custom_function_not_exceeding_tt_detail_threshold called
function_exceeding_tt_detail_threshold called
custom_function_not_exceeding_tt_detail_threshold called
No alarms and no surprises.
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
        "name": "Custom\/custom_function_not_exceeding_tt_detail_threshold",
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
        "code.function": "custom_function_not_exceeding_tt_detail_threshold"
      }
    ]
  ]
]
*/

function function_exceeding_tt_detail_threshold() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  time_nanosleep(0, 100 * 1000); // 100 microseconds should be enough (= 2 x newrelic.special.expensive_node_min)
  echo 'function_exceeding_tt_detail_threshold called' . PHP_EOL;
}

function custom_function_not_exceeding_tt_detail_threshold() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  echo 'custom_function_not_exceeding_tt_detail_threshold called' . PHP_EOL;
}

// These calls will not be a span in span events
function_exceeding_tt_detail_threshold();
custom_function_not_exceeding_tt_detail_threshold();

// Add a custom wrapper to ensure that all future calls will be recorded as spans in span events
newrelic_add_custom_tracer('custom_function_not_exceeding_tt_detail_threshold');

// This call will not be a span in span events
function_exceeding_tt_detail_threshold();

// This call will be a span in span events
custom_function_not_exceeding_tt_detail_threshold();

echo 'No alarms and no surprises.'  . PHP_EOL;
