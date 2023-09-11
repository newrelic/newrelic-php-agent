<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should properly instrument Wordpress apply_filters hooks.
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
add filter
add filter
add filter
f: string1
h: string3
g: string2
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
    [{"name": "Framework/WordPress/Hook/h"},                          [1, "??", "??", "??", "??", "??"]],
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

// Simple mock of wordpress's apply_filter()
// In a real Wordpress app, the $tag is not what is eventually
// called by the call_user_func_array. We do this for test simplicity
function apply_filters($tag, ...$args) {
    call_user_func_array($tag, $args);
}

function add_filter($tag, $callback) {
    echo "add filter\n";
}

//Simple mock of wordpress's get_theme_roots
function get_theme_roots() {
}

function h($str) {
    echo "h: ";
    echo $str;
    echo "\n";
    throw new Exception("Test Exception");
}

function g($str) {
    echo "g: ";
    echo $str;
    echo "\n";
}

function f($str) {
    echo "f: ";
    echo $str;
    echo "\n";
    try {
        apply_filters("h", "string3");
    } catch (Exception $e) {
        apply_filters("g", "string2");
    }
}

// Due to the mock simplification described above, the hook
// is not used in this test, and the callback is treated as the hook
add_filter("hook", "f");
add_filter("hook", "g");
add_filter("hook", "h");
apply_filters("f", "string1");
