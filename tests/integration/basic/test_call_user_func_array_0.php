<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Exercise the instrumentation for call_user_func_array(), which is hooked when
CodeIgniter or Drupal is detected/forced.
*/

/*INI
newrelic.framework=codeigniter   ; cause agent to hook call_user_func_array
*/

/*EXPECT
foo=17
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/foo"},                                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/foo",
      "scope":"OtherTransaction/php__FILE__"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/CodeIgniter/forced"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},                [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




function foo($n) {
  printf("foo=%d\n", $n);
}

function run_test() {
  /*
   * newrelic_add_custom_tracer doesn't instrument internal php functions.
   */
  newrelic_add_custom_tracer("foo");

  call_user_func_array("foo", array(17));
}

run_test();
