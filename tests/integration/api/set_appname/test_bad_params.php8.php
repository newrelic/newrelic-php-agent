<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_set_appname with bad parameters.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
newrelic_set_appname no params
newrelic_set_appname bad appname
newrelic_set_appname bad license
newrelic_set_appname bad transmit
newrelic_set_appname too many params
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"see_me"},                                 [1, 1, 1, 1, 1, 1]],
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/custom_metric"},       [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/set_appname/before"},  [5, 0, 0, 0, 0, 0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$appname = ini_get("newrelic.appname");
$license = ini_get("newrelic.license");

newrelic_custom_metric('see_me', 1e+3);

try {
  newrelic_set_appname();
} catch (ArgumentCountError $e) {
  echo 'newrelic_set_appname no params',"\n";
}

try {
  $result = @newrelic_set_appname(array());
} catch (TypeError $e) {
  echo 'newrelic_set_appname bad appname',"\n";
}

try {
  @newrelic_set_appname($appname, array());
} catch (TypeError $e) {
  echo 'newrelic_set_appname bad license',"\n";
}

try {
  @newrelic_set_appname($appname, $license, array());
} catch (TypeError $e) {
  echo 'newrelic_set_appname bad transmit',"\n";
}

try {
  @newrelic_set_appname($appname, $license, true, 1);
} catch (ArgumentCountError $e) {
  echo 'newrelic_set_appname too many params',"\n";
}
