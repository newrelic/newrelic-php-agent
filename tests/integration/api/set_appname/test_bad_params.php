<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_set_appname returns false when given bad parameters.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
*/

/*EXPECT_REGEX
.*Warning:.*newrelic_set_appname\(\) expects at least 1 parameter, 0 given.*
ok - newrelic_set_appname no params
ok - newrelic_set_appname bad appname
ok - newrelic_set_appname bad license
ok - newrelic_set_appname bad transmit
ok - newrelic_set_appname too many params
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Errors/all"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"see_me"},                                 [1, 1, 1, 1, 1, 1]],
    [{"name":"Supportability/api/custom_metric"},       [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/before"},  [5, 0, 0, 0, 0, 0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$appname = ini_get("newrelic.appname");
$license = ini_get("newrelic.license");

newrelic_custom_metric('see_me', 1e+3);

$result = newrelic_set_appname();
tap_refute($result, 'newrelic_set_appname no params');

$result = @newrelic_set_appname(array());
tap_refute($result, 'newrelic_set_appname bad appname');

$result = @newrelic_set_appname($appname, array());
tap_refute($result, 'newrelic_set_appname bad license');

$result = @newrelic_set_appname($appname, $license, array());
tap_refute($result, 'newrelic_set_appname bad transmit');

$result = @newrelic_set_appname($appname, $license, true, 1);
tap_refute($result, 'newrelic_set_appname too many params');
