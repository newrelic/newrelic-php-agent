<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that span event attributes are disabled according to the configuration.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.detail = false
newrelic.span_events_enabled=1
newrelic.span_events.attributes.enabled = false
newrelic.cross_application_tracer.enabled = false
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
ok - string attribute not added
ok - int attribute not added
ok - bool attribute not added
ok - double attribute not added
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
      {},
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
    [{"name":"Supportability/api/add_custom_span_parameter"},       [4, 0, 0, 0, 0, 0]]
  ]
]
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

newrelic_add_custom_tracer("a");

function a() {
  tap_refute(newrelic_add_custom_span_parameter("string", "str"), "string attribute not added");
  tap_refute(newrelic_add_custom_span_parameter("int", 7), "int attribute not added");
  tap_refute(newrelic_add_custom_span_parameter("bool", false), "bool attribute not added");
  tap_refute(newrelic_add_custom_span_parameter("double", 1.5), "double attribute not added");
}

a();
