<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
 * Tests a mock of Drupals new hook invoking methods, introduced in Drupal 9.4+
*/

/*SKIPIF
if (version_compare(phpversion(), '7.4', '<')) {
    die("skip: PHP >= 7.4 required\n");
}
*/

/*INI
newrelic.framework = drupal8
*/

/*EXPECT
a1
b1
b2
a3
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/hook_1"},                         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/hook_2"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/hook_3"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Module/module_a"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Module/module_b"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal8/forced"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once __DIR__.'/mock_module_handler.php';

// This specific API is needed for us to instrument the ModuleHandler
class Drupal {
    public function moduleHandler() {
        return new Drupal\Core\Extension\ModuleHandler();
    }
}

function module_a_hook_1() {
    echo "a1\n";
}
function module_a_hook_3() {
    echo "a3\n";
}
function module_b_hook_1() {
    echo "b1\n";
}
function module_b_hook_2() {
    echo "b2\n";
}

// Various methods to pass to invokeAllWith
function invoke_callback(callable $hook, string $module) {
    $hook();
}
class Invoker {
    public function invoke(callable $hook, string $module) {
        $hook();
    }
}

// Create module handler
$drupal = new Drupal();
$handler = $drupal->moduleHandler();

// Test lambda calback
$handler->invokeAllWith("hook_1", function (callable $hook, string $module) {
    $hook();
});

// Test string and reference callback
$func_name = "invoke_callback";
$func_name_ref =& $func_name;
$handler->invokeAllWith("hook_2", $func_name_ref);

//Test callable array callback
$invoker = new Invoker();
$handler->invokeAllWith("hook_3", [$invoker, "invoke"]);
