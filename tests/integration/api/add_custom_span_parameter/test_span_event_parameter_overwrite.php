<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Test that span attributes overwrite transaction event attributes.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = false
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
ok - transaction event attribute added
ok - span event attribute added
ok - span event attribute added
ok - transaction event attribute added
ok - transaction event attribute added
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
        "int": 100,
        "string": "from span event",
        "bool": false
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
      "scope": "OtherTransaction/php__FILE__"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},               [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/add_custom_parameter"},            [3, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/add_custom_span_parameter"},       [2, 0, 0, 0, 0, 0]]
  ]
]
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

newrelic_add_custom_tracer("a");

function a() {
  tap_assert(newrelic_add_custom_parameter("string", "from transaction event"), "transaction event attribute added");
  tap_assert(newrelic_add_custom_span_parameter("string", "from span event"), "span event attribute added");

  tap_assert(newrelic_add_custom_span_parameter("int", 100), "span event attribute added");
  tap_assert(newrelic_add_custom_parameter("int", 333), "transaction event attribute added");

  tap_assert(newrelic_add_custom_parameter("bool", false), "transaction event attribute added");
}

a();
