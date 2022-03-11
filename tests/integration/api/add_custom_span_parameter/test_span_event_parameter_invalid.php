<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that invalid span event attributes are handled gracefully.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.detail = 0
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_REGEX
ok - no args
ok - no value
[\r\n]*.*Warning:.*newrelic_add_custom_span_parameter: expects parameter to be scalar, array given.*
ok - array value
[\r\n]*.*Warning.*newrelic_add_custom_span_parameter: expects parameter to be scalar, array given.*
ok - hash value
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
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
        "error.message": "??",
        "error.class": "E_WARNING"
      }
    ]
  ]
]
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

tap_refute(newrelic_add_custom_span_parameter(), "no args");
tap_refute(newrelic_add_custom_span_parameter("key"), "no value");
tap_refute(newrelic_add_custom_span_parameter("key", array(1, 2, 3)), "array value");
tap_refute(newrelic_add_custom_span_parameter("key", array("x" => "y")), "hash value");
