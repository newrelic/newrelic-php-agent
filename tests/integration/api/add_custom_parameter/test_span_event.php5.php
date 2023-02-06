<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that custom transaction attributes are added to span events.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = false
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.code_level_metrics.enabled=false
*/

/*EXPECT
ok - string attribute added
ok - int attribute added
ok - bool attribute added
ok - double attribute added
 */

/*EXPECT_ANALYTICS_EVENTS
 [
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": false
      },
      {
        "double": 1.50000,
        "bool": false,
        "int": 7,
        "string": "str"
      },
      {
      }
    ]
  ]
]
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
        "transaction.name": "OtherTransaction/php__FILE__"
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
      {
        "double": 1.50000,
        "bool": false,
        "int": 7,
        "string": "str"
      },
      {}
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

newrelic_add_custom_tracer("a");

function a() {
  tap_assert(newrelic_add_custom_parameter("string", "str"), "string attribute added");
  tap_assert(newrelic_add_custom_parameter("int", 7), "int attribute added");
  tap_assert(newrelic_add_custom_parameter("bool", false), "bool attribute added");
  tap_assert(newrelic_add_custom_parameter("double", 1.5), "double attribute added");
}

a();
