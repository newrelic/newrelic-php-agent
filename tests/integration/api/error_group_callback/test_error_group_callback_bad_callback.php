<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_error_group_callback() API handling when passed a non-function object.

When a non-function object is passed to newrelic_set_error_group_callback(), it will result in an error
and the callback will not be registered.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 treats this error as an exception.\n");
}
*/

/*EXPECT_REGEX
Fatal error: Uncaught TypeError: newrelic_set_error_group_callback\(\): Argument #1 \(\$callback\) must be a valid callback, function "This isn't a callback!" not found or invalid function name in .*test_error_group_callback_bad_callback.php:120
Stack trace:
#0 .*test_error_group_callback_bad_callback.php\(120\): newrelic_set_error_group_callback\('This isn't a ca...'\)
#1 {main}
  thrown in .*test_error_group_callback_bad_callback.php on line 120
*/

/*EXPECT_METRICS 
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},      [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name": "Errors/OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name": "Errors/all"},                                                [1, "??", "??", "??", "??", "??"]],
    [{"name": "Errors/allOther"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},        [1, "??", "??", "??", "??", "??"]],
    [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},   [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/api/set_error_group_callback"},               [1, 0, 0, 0, 0, 0]]
  ]
]
*/

/*EXPECT_ERROR_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 100,
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "TypeError",
        "error.message": "Uncaught exception 'TypeError' with message 'newrelic_set_error_group_callback(): Argument #1 ($callback) must be a valid callback, function \"This isn't a callback!\" not found or invalid function name' in __FILE__:??",
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

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction\/php__FILE__",
      "Uncaught exception 'TypeError' with message 'newrelic_set_error_group_callback(): Argument #1 ($callback) must be a valid callback, function \"This isn't a callback!\" not found or invalid function name' in __FILE__:??",
      "TypeError",
      {
        "stack_trace": [
          " in newrelic_set_error_group_callback called at __FILE__ (??)"
        ],
        "agentAttributes": {},
        "intrinsics": {
          "totalTime": "??",
          "cpu_time": "??",
          "cpu_user_time": "??",
          "cpu_sys_time": "??",
          "guid": "??",
          "sampled": true,
          "priority": "??",
          "traceId": "??"
        }
      }
    ]
  ]
]
*/

function alpha()
{
  newrelic_notice_error(new Exception('Sample Exception'));
}

$not_a_callback = "This isn't a callback!";

newrelic_set_error_group_callback($not_a_callback);

alpha();
