<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Exercise the instrumentation for the dl() function.
*/

/*INI
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/InstrumentedFunction/dl"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



/*EXPECT_TRACED_ERRORS
[
  "??",
  [
    [
      "??",
      "OtherTransaction/php__FILE__",
      "dl(): Temporary module name should contain only filename",
      "E_WARNING",
      {
        "stack_trace": [
          " in dl called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

if (!ini_get("enable_dl") || ini_get("safe_mode")) {
  die("skip: loading extensions is not permitted.\n");
}

date_default_timezone_set('America/Los_Angeles');

dl("/dev/null");
