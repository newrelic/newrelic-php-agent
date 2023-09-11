<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_error_group_callback() API for non-Web errors.
*/

/*EXPECT_REGEX
request_uri => 
path => test_error_group_callback_error_non_web.php
method => 
status_code => 0

klass => E_USER_ERROR
message => I'M COVERED IN BEES
file => .*test_error_group_callback_error_non_web.php
stack => \[" in trigger_error called at .*test_error_group_callback_error_non_web.php \(.*\)"," in alpha called at .*test_error_group_callback_error_non_web.php \(.*\)"\]

Fatal error: I'M COVERED IN BEES in .*test_error_group_callback_error_non_web.php on line .*
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
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "TransactionError",
        "timestamp": "??",
        "error.class": "E_USER_ERROR",
        "error.message": "I'M COVERED IN BEES",
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
      {
        "error.group.name": "CUSTOM ERROR GROUP NAME"
      }
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
      "I'M COVERED IN BEES",
      "E_USER_ERROR",
      {
        "stack_trace": [
          " in trigger_error called at __FILE__ (??)",
          " in alpha called at __FILE__ (??)"
        ],
        "agentAttributes": {
          "error.group.name": "CUSTOM ERROR GROUP NAME"
        },
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

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

function alpha()
{
  trigger_error("I'M COVERED IN BEES", E_USER_ERROR);
}

$callback = function($txndata, $errdata) 
{
    foreach($txndata as $tkey => $tdata) {
      echo "$tkey => $tdata\n";
    };

    echo "\n";

    foreach($errdata as $ekey => $edata) {
      echo "$ekey => $edata\n";
    };

    $fingerprint = "CUSTOM ERROR GROUP NAME";
    return $fingerprint;
};

$result = newrelic_set_error_group_callback($callback);
tap_assert($result, "callback registered");

alpha();
