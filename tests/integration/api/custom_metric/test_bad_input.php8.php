<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Calling newrelic_custom_metric() with the wrong arguments should return FALSE
and no metric should be added.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*EXPECT
ok - should reject zero args
ok - should reject one arg
ok - should reject non-string name
ok - should reject non-numeric value (string)
ok - should reject non-numeric value (array)
ok - should reject infinity
ok - should reject NaN
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/custom_metric"},       [7, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$result = @newrelic_custom_metric();
tap_refute($result, 'should reject zero args');

$result = @newrelic_custom_metric('Custom/Metric');
tap_refute($result, 'should reject one arg');

/*
 * In PHP 8, internal function parameters have types and value validations enforced and will now
 * throw TypeError exceptions if the expected type or value is not allowed. Prior to PHP 8, this
 * only resulted in a PHP warning.
 */
try {
  @newrelic_custom_metric(array(), 1.0);
} catch (TypeError $e) {
  echo 'ok - should reject non-string name',"\n";
}

try {
  @newrelic_custom_metric('Custom/Metric', 'bad');
} catch (TypeError $e) {
  echo 'ok - should reject non-numeric value (string)',"\n";
}

try {
  @newrelic_custom_metric('Custom/Metric', array());
} catch (TypeError $e) {
  echo 'ok - should reject non-numeric value (array)',"\n";
}

$result = @newrelic_custom_metric('Custom/Metric', INF);
tap_refute($result, 'should reject infinity');

$result = @newrelic_custom_metric('Custom/Metric', NAN);
tap_refute($result, 'should reject NaN');
