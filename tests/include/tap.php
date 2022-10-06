<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

// Simple assertions for producing TAP output.
//
// See: http://testanything.org/tap-specification.html

// Assert $actual to be strictly TRUE.
function tap_assert($actual, $msg) {
  if (true === $actual) {
    tap_ok($msg);
  } else {
    tap_not_ok($msg, true, $actual);
  }
}

// Assert $actual to be strictly FALSE.
function tap_refute($actual, $msg) {
  if (false === $actual) {
    tap_ok($msg);
  } else {
    tap_not_ok($msg, false, $actual);
  }
}

// Assert $actual strictly equal to $expect.
function tap_equal($expect, $actual, $msg) {
  if ($expect === $actual) {
    tap_ok($msg);
  } else {
    tap_not_ok($msg, $expect, $actual);
  }
}

// Assert $actual strictly unequal to $expect.
function tap_not_equal($expect, $actual, $msg) {
  if ($expect !== $actual) {
    tap_ok($msg);
  } else {
    tap_not_ok($msg, $expect, $actual, true);
  }
}

// Assert the elements of $actual strictly equal the elements in $expect
// regardless of their order.
function tap_equal_unordered($expect, $actual, $msg) {
  if (is_array($expect) && is_array($actual)) {
    ksort($expect);
    ksort($actual);
  }
  tap_equal($expect, $actual, $msg);
}

function tap_matches($pattern, $actual, $ok_msg) {
  if (preg_match($pattern, $actual)) {
    tap_ok($ok_msg);
  } else {
    tap_not_ok("value does not match pattern", $pattern, $actual);
  }
}

// Assert the elements of $actual are the same as the elements in $expect
// regarless of the values order
function tap_equal_unordered_values($expect, $actual, $msg) {
  if (is_array($expect) && is_array($actual)) {
    sort($expect);
    sort($actual);
  }
  tap_equal($expect, $actual, $msg);
}

// Prints a test pass in TAP format.
function tap_ok($msg) {
  echo "ok - ${msg}\n";
}

// Prints a test failure in TAP format.
function tap_not_ok($msg, $expect, $actual, $negated = false) {
  echo "not ok - ${msg}\n";

  $expect_str = var_export($expect, true);
  $actual_str = var_export($actual, true);

  if ($negated) {
    tap_diagnostic('expected: value != ' . $expect_str);
  } else {
    tap_diagnostic('expected: ' . $expect_str);
  }

  tap_diagnostic('got: ' . $actual_str);
}

// Prints a TAP formatted diagnostic message.
function tap_diagnostic($msg) {
  foreach(explode("\n", $msg) as $line) {
    echo '# ' . $line . "\n";
  }
}
