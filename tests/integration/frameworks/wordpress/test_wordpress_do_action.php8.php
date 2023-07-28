<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should properly instrument Wordpress do_action hooks.
With OAPI, the agent will not generate external segment metrics in all cases where an exception occurred
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not OAPI\n");
}
*/

/*INI
newrelic.framework = wordpress
*/

/*EXPECT
g
f
h
g
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},      [1, "??", "??", "??", "??", "??"]],
    [{"name": "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name": "Framework/WordPress/Hook/f"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name": "Framework/WordPress/Hook/g"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name": "OtherTransactionTotalTime/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/enabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/enabled"},          [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/framework/WordPress/forced"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

// Simple mock of wordpress's do_action()
function do_action($tag, ...$args) {
    call_user_func_array($tag, $args);
}

function h() {
    echo "h\n";
    throw new Exception("Test Exception");
}

function g() {
    echo "g\n";
}

function f() {
    echo "f\n";
    try {
        do_action("h");
    } catch (Exception $e){
        do_action("g");
    }
}

/* 
 * Initiates a non-flattened call stack of internal->user_code
 * to ensure that cufa instrumentation properly handles skipping
 * opline lookups of internal functions
 */
$function = new ReflectionFunction('g');
$function->invoke();


do_action("f");
