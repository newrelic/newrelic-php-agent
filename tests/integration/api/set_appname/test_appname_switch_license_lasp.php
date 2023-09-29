<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_set_appname prevents a new application being connected when
called with two parameters when Language Agent Security Policy (LASP) 
is enabled.
*/

/*INI
newrelic.security_policies_token = 00000000
*/

/*EXPECT_REGEX
.*Warning:.*newrelic_set_appname: when a security_policies_token is present in the newrelic.ini file, it is not permitted to call newrelic_set_appname\(\) with a non-empty license key. LASP does not permit changing accounts during runtime. Consider using "" for the second parameter.*
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/custom_metric"},                    [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/start_transaction"},                [1, 0, 0, 0, 0, 0]],
    [{"name":"see_me"},                                              [1, 1, 1, 1, 1, 1]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$appname = ini_get("newrelic.appname");

newrelic_custom_metric('ignore_me', 1e+3);
$result = newrelic_set_appname($appname, "0000000000000000000000000000000000000000", 0);

newrelic_custom_metric('also_ignore_me', 1e+3);

// We should have to start a new transaction here, since the
// newrelic_set_appname() will have terminated the previous transaction.
newrelic_start_transaction($appname);
newrelic_custom_metric('see_me', 1e+3);
