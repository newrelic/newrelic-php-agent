<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When transaction tracer details are disabled, test that a call to a function
that is not custom wrapped and records a PHP warning does not appear as a span
in span events, and that the error attributes are on the root span instead.
This is the PHP 7.x variant, where the function has no segment of its own, so
the error, and the span ID that goes into the error event, belong to the root
segment. The variant for PHP 8.x is test_tt_detail_off_with_warning.php.
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
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 50us
log_errors=0
display_errors=1
*/

/*EXPECT_REGEX
^\s*(PHP )?Warning:\s*I'm covered in bees!!! in .*? on line [0-9]+\s*$
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
      {
        "error.message": "I'm covered in bees!!!",
        "error.class": "E_USER_WARNING"
      }
    ]
  ]
]
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
        "error.class": "E_USER_WARNING",
        "error.message": "I'm covered in bees!!!",
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

function custom_function_not_exceeding_tt_detail_threshold_but_warning() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
  trigger_error("I'm covered in bees!!!", E_USER_WARNING);
}

custom_function_not_exceeding_tt_detail_threshold_but_warning();
