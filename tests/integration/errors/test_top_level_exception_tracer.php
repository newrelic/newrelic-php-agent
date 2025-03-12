<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not remove a custom tracer from a function when it is no longer
used as an exception handler.
*/

/*INI
*/

/*EXPECT
NULL
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/handler"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/handler",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/*EXPECT_ERROR_EVENTS
null
*/

function handler($var) {
  var_dump($var);
}

newrelic_add_custom_tracer('handler');

set_exception_handler('handler');
restore_exception_handler();

handler(null);
