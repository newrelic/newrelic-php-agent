<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test adding a custom metric. The metric should be added as unscoped only, and
the agent should correctly calculate the count, sum, min, max and sum-of-
squares.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT
ok - min added successfully
ok - max added successfully
ok - median added successfully
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/Application/Metric"},              [3,  7.0,  7.0,  1.0,  4.0, 21.0]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/custom_metric"},       [3, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

/*
 * Custom metric values are scaled by a factor of 1e+3.
 *
 * e.g. Metrics representing a duration should be reported to New Relic
 * as fractional seconds, and therefore the value passed to the api
 * should be in milliseconds.
 */

$result = newrelic_custom_metric('Custom/Application/Metric', 1e+3);
tap_assert($result, 'min added successfully');

$result = newrelic_custom_metric('Custom/Application/Metric', 4e+3);
tap_assert($result, 'max added successfully');

$result = newrelic_custom_metric('Custom/Application/Metric', 2e+3);
tap_assert($result, 'median added successfully');
