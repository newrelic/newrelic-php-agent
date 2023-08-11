<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Dev test for newrelic_set_error_group_callback api
*/

/*EXPECT 
*/

/*EXPECT_METRICS 
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},      [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},             [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},                [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/api/set_error_group_callback"},               [2, 0, 0, 0, 0, 0]]
  ]
]
*/

// require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$callback_bad = function($arg1)
{
    echo "$arg1";
    $fingerprint = "THIS SHOULD FAIL";
    return $fingerprint;
};

$callback = function($arg1, $arg2) 
{
    echo "$arg1, $arg2";
    $fingerprint = "CUSTOM ERROR GROUP NAME";
    return $fingerprint;
};

newrelic_set_error_group_callback($callback_bad);
newrelic_set_error_group_callback($callback);

