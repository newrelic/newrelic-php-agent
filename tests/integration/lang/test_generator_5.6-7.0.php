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

if (version_compare(PHP_VERSION, '5.5.0', '<')) {
  die("skip: generators not available");
}

if (version_compare(PHP_VERSION, '7.1', '>=')) {
  die("skip: generator priming changed in PHP 7.1");
}
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
    [{"name":"Custom/xrange"},                              [11, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                       [1,  "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},               [1,  "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                  [1,  "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},      [1,  "??", "??", "??", "??", "??"]],
    [{"name":"Custom/xrange","scope":"OtherTransaction/php__FILE__"},
                                                            [11, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/InstrumentedFunction/xrange"}, [11, "??", "??", "??", "??", "??"]],
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
