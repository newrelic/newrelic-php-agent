<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that a custom span parameter set from a user function is reported when
newrelic.transaction_tracer.detail = 0. The attribute is stored on the segment
of the function, and it only reaches the backend as an attribute of that
function's span, so the segment must not be dropped for being a user function
the agent has no other reason to keep.
This is the PHP 7.x variant, where the function has no span of its own and the
parameter is set on the root span. The variant for PHP 8.x is
test_span_event_parameter_in_function_tt_detail_off.php.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", ">=")) {
  die("skip: test for non-oapi agent only\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
*/

/*EXPECT_SPAN_EVENTS_LIKE
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
      {
        "key": "value"
      },
      {}
    ]
]
*/

function function_with_span_parameter()
{
    newrelic_add_custom_span_parameter("key", "value");
}

function_with_span_parameter();
