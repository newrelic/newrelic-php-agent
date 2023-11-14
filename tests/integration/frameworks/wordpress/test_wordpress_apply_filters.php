<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should properly instrument Wordpress apply_filters hooks.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "5.6", "<")) {
  die("skip: PHP < 5.6 argument unpacking not supported\n");
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
add filter
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
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/framework/WordPress/forced"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once __DIR__.'/mock_hooks.php';

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
    // For OAPI: attempt to overwrite the currently executing transient wrapper
    add_filter("hook", "f");
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
