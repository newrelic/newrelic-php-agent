<?php

/*DESCRIPTION
Test that a simple request through drupal_http_request gets instrumented.
With OAPI, the agent will not generate external segment metrics in all cases where an exception occurred
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not OAPI\n");
}
*/

/*EXPECT
module_hook_with_arg(arg=[arg_value])
g
h
f
h
f
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/hook_with_arg"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/f"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/g"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Module/module"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"},              [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function module_invoke_all($f) {
    $args = func_get_args();
    unset($args[0]);
    call_user_func_array("module_" . $f, $args);
}

function module_h() {
    echo "h\n";
    throw new Exception("Test Exception");
}

function module_f() {
    try {
        module_invoke_all("h");
    } catch (Exception $e) {
        echo "f\n";
    }
}

function module_g() {
    echo "g\n";
    module_f();
}

function module_hook_with_arg($arg) {
        echo "module_hook_with_arg(arg=[$arg])\n";
}

module_invoke_all("hook_with_arg", "arg_value");
module_invoke_all("g");
module_invoke_all("f");
