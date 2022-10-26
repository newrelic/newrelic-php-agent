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
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
*/

/*INI
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/custom_metric"},                     [7, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$result = @newrelic_custom_metric();
tap_refute($result, 'should reject zero args');

$result = @newrelic_custom_metric('Custom/Metric');
tap_refute($result, 'should reject one arg');

$result = @newrelic_custom_metric(array(), 1.0);
tap_refute($result, 'should reject non-string name');

$result = @newrelic_custom_metric('Custom/Metric', 'bad');
tap_refute($result, 'should reject non-numeric value (string)');

$result = @newrelic_custom_metric('Custom/Metric', array());
tap_refute($result, 'should reject non-numeric value (array)');

$result = @newrelic_custom_metric('Custom/Metric', INF);
tap_refute($result, 'should reject infinity');

$result = @newrelic_custom_metric('Custom/Metric', NAN);
tap_refute($result, 'should reject NaN');
