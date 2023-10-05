<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_error_group_callback() API handling when a user callback returns a null value instead of a non-empty string.

When a value other than a non-empty string is returned by a user-registered error fingerprint callback,
the callback will still be registered and called, but the `error.group.name` attribute will not be generated.
*/

/*EXPECT_REGEX
ok - callback registered
request_uri => 
path => test_error_group_callback_bad_return_null.php
method => 
status_code => 0

klass => Exception
message => Sample Exception
file => .*test_error_group_callback_bad_return_null.php
stack => \[" in alpha called at .*test_error_group_callback_bad_return_null.php \(.*\)"\]
*/

/*EXPECT_METRICS 
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/notice_error"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_error_group_callback"},                [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},        [1, "??", "??", "??", "??", "??"]]
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
        "error.class": "Exception",
        "error.message": "Noticed exception 'Exception' with message 'Sample Exception' in __FILE__:??",
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
      "Noticed exception 'Exception' with message 'Sample Exception' in __FILE__:??",
      "Exception",
      {
        "stack_trace": [
          " in alpha called at __FILE__ (??)"
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

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

function alpha()
{
  newrelic_notice_error(new Exception('Sample Exception'));
}

$callback_bad_return = function($txndata, $errdata) 
{
    foreach($txndata as $tkey => $tdata) {
      echo "$tkey => $tdata\n";
    };

    echo "\n";

    foreach($errdata as $ekey => $edata) {
      echo "$ekey => $edata\n";
    };

    $fingerprint = null;
    return $fingerprint;
};

$result = newrelic_set_error_group_callback($callback_bad_return);

tap_assert($result, "callback registered");

alpha();
