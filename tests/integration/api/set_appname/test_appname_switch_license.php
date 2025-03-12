<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_set_appname works with two parameters.
*/

/*EXPECT
ok - newrelic_set_appname appname and license
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [2, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [2, "??", "??", "??", "??", "??"]],
    [{"name":"see_me"},                                               [1, 1, 1, 1, 1, 1]],
    [{"name":"see_me_also"},                                          [1, 1, 1, 1, 1, 1]],
    [{"name":"Supportability/api/custom_metric"},                     [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/before"},                [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/after"},                 [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/switched_license"},      [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/with_license"},          [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [2, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [2, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [2, "??", "??", "??", "??", "??"]]
  ]
]
*/


require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$appname = ini_get("newrelic.appname");
$license = ini_get("newrelic.license");

newrelic_custom_metric('ignore_me', 1e+3);
$result = newrelic_set_appname($appname, "0000000000000000000000000000000000000000", 0);

newrelic_custom_metric('see_me', 1e+3);
$result = newrelic_set_appname($appname, $license, 1);
tap_assert($result, 'newrelic_set_appname appname and license');

newrelic_custom_metric('see_me_also', 1e+3);
