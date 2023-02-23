<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_set_appname works as expected when the transmit bool is
set to true.
*/

/*INI
*/

/*EXPECT
ok - newrelic_set_appname transmit=true
ok - newrelic_set_appname transmit=1
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_appname/after"},                 [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/before"},                [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/with_license"},          [2, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [3, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$appname = ini_get("newrelic.appname");
$license = ini_get("newrelic.license");

$result = newrelic_set_appname($appname, $license, true);
tap_assert($result, 'newrelic_set_appname transmit=true');

$result = newrelic_set_appname($appname, $license, 1);
tap_assert($result, 'newrelic_set_appname transmit=1');
