<?php

/*DESCRIPTION
Test that a simple request through drupal_http_request gets instrumented.
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
*/

/*EXPECT
g
f
f
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/f"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Hook/g"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Framework/Drupal/Module/module"},         [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal/forced"}, [1,    0,    0,    0,    0,    0]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function module_invoke_all($f) {
    call_user_func_array("module_" . $f, array());
}

function module_f() {
    echo "f\n";
}

function module_g() {
    echo "g\n";
    module_f();
}

module_invoke_all("g");
module_invoke_all("f");
