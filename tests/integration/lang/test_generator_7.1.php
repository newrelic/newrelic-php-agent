<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should correctly instrument generators.
*/

/*SKIPIF
<?php

if (version_compare(PHP_VERSION, '7.1', '<')) {
  die("skip: generators either not available or with different behaviour");
}
*/

/*INI
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
1,2,3,4,5,6,7,8,9,10,
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/xrange"},                              [12, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                       [1,  "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},               [1,  "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                  [1,  "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},      [1,  "??", "??", "??", "??", "??"]],
    [{"name":"Custom/xrange","scope":"OtherTransaction/php__FILE__"},
                                                            [12, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/InstrumentedFunction/xrange"}, [12, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},       [1,  "??", "??", "??", "??", "??"]]
  ]
]
*/

function factorial($n)
{
  if ($n <= 0) {
    return 1;
  } else {
    return $n * factorial($n-1);
  }
}

function defeat_inlining_and_tail_recursion()
{
  factorial((rand() >> 3) & 0x7);
}

/*
 * Generators are a new feature in PHP 5.5
 * http://php.net/manual/en/language.generators.overview.php
 */

function xrange($start, $limit, $step = 1) {
  for ($i = $start; $i <= $limit; $i += $step) {
    defeat_inlining_and_tail_recursion();
    yield $i;
    defeat_inlining_and_tail_recursion();
  }
}

newrelic_add_custom_tracer("xrange");

foreach (xrange(1, 10, 1) as $number) {
  echo "$number,";
}

echo "\n";
