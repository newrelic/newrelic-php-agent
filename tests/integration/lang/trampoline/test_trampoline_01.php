<?php
/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that trampoline functions do not blow up.
*/

/*INI
newrelic.code_level_metrics.enabled = true
*/

/*EXPECT_METRICS_EXIST
Custom/trampoline_factorial, 42
*/

/*EXPECT_TRACED_ERRORS null*/

/*EXPECT
ok - trampoline('trampoline_factorial', 42)
*/

require_once __DIR__ . '/../../../include/tap.php';

newrelic_add_custom_tracer('trampoline_factorial');

function trampoline_factorial($n, $acc = 1)
{
  if ($n == 1) {
    return $acc;
  }
  return function () use ($n, $acc) {
    return trampoline_factorial($n - 1, $n * $acc);
  };
}

function trampoline(callable $c, ...$args)
{
  while (is_callable($c)) {
    $c = $c(...$args);
  }
  return $c;
}

tap_equal(1.4050061177528801E+51, trampoline('trampoline_factorial', 42), "trampoline('trampoline_factorial', 42)");
