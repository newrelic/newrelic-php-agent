<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that attributes are added to span events. If the transaction event has an
attribute with the same name as the currently executing span event, the value
from the span MUST be kept.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = false
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.code_level_metrics.enabled=false
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", ">=")) {
  die("skip: PHP 7+ not supported\n");
}
*/

/*EXPECT
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - int attribute added
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
      {
        "double": 1.50000,
        "bool": false,
        "int": 1,
        "string": "span str"
      },
      {}
    ]
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/a"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/a",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},               [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/add_custom_span_parameter"},       [3, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/add_custom_parameter"},            [5, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

newrelic_add_custom_tracer("a");

function a() {
  tap_assert(newrelic_add_custom_span_parameter("string", "span str first"), "string attribute added");
  tap_assert(newrelic_add_custom_span_parameter("string", "span str"), "string attribute added");
  tap_assert(newrelic_add_custom_parameter("string", "str first"), "string attribute added");
  tap_assert(newrelic_add_custom_parameter("string", "str"), "string attribute added");
  tap_assert(newrelic_add_custom_parameter("int", 7), "int attribute added");
  tap_assert(newrelic_add_custom_span_parameter("int", 1), "int attribute added");
  tap_assert(newrelic_add_custom_parameter("bool", false), "bool attribute added");
  tap_assert(newrelic_add_custom_parameter("double", 1.5), "double attribute added");
}

a();
