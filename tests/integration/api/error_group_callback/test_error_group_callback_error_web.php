<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_error_group_callback() API for Web errors.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.4", ">=")) {
  die("skip: newer test for PHPs 8.4+\n");
}
*/


/*ENVIRONMENT
REQUEST_METHOD=GET
QUERY_STRING=foo=1&bar=2
*/

/*EXPECT_REGEX
ok - callback registered
request_uri => \/test_error_group_callback_error_web.php\?foo=1&bar=2
path => .*test_error_group_callback_error_web.php
method => GET
status_code => 200

klass => E_USER_ERROR
message => I'M COVERED IN BEES
file => .*test_error_group_callback_error_web.php
stack => \[" in trigger_error called at .*test_error_group_callback_error_web.php \(.*\)"," in alpha called at .*test_error_group_callback_error_web.php \(.*\)"\]
<br \/>
<b>Fatal error<\/b>:  I'M COVERED IN BEES in <b>.*test_error_group_callback_error_web.php<\/b> on line <b>.*<\/b><br \/>
*/

/*EXPECT_METRICS 
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Apdex"},                                                      [0, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex/Uri__FILE__"},                                          [0, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allWeb"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/WebTransaction/Uri__FILE__"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allWeb"},                                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allWeb"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_error_group_callback"},                [1, 0, 0, 0, 0, 0]],
    [{"name":"WebTransaction"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},                 [1, "??", "??", "??", "??", "??"]]
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
        "error.class": "E_USER_ERROR",
        "error.message": "I'M COVERED IN BEES",
        "transactionName": "WebTransaction\/Uri__FILE__",
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
        "response.headers.contentType": "application\/json",
        "http.statusCode": 200,
        "response.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "error.group.name": "CUSTOM ERROR GROUP NAME",
        "SERVER_NAME": "127.0.0.1",
        "request.method": "GET",
        "request.headers.host": "127.0.0.1"
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
      "WebTransaction\/Uri__FILE__",
      "I'M COVERED IN BEES",
      "E_USER_ERROR",
      {
        "stack_trace": [
          " in trigger_error called at __FILE__ (??)",
          " in alpha called at __FILE__ (??)"
        ],
        "agentAttributes": {
          "response.headers.contentType": "application\/json",
          "http.statusCode": 200,
          "response.statusCode": 200,
          "httpResponseCode": "200",
          "request.uri": "__FILE__",
          "error.group.name": "CUSTOM ERROR GROUP NAME",
          "SERVER_NAME": "127.0.0.1",
          "request.method": "GET",
          "request.headers.host": "127.0.0.1"
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
        },
        "request_uri": "__FILE__"
      },
      "?? transaction ID"
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

header('Content-Type: text/html');
header('Content-Type: application/json');

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
