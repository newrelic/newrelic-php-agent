<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should properly instrument Wordpress do_action hooks. Since the
mocked hooks are detected by the agent as WordPress core (plugin_from_function 
returns NULL), and WordPress core callbacks are not instrumented by default,
therefore newrelic.framework.wordpress.core needs to be set to true for the
agent to generate the hooks metrics.
*/

/*SKIPIF
*/

/*INI
newrelic.framework = wordpress
newrelic.framework.wordpress.hooks_threshold = 0
newrelic.framework.wordpress.core = true
*/

/*EXPECT
add filter
add filter
add filter
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

// Due to the mock simplification described above, the hook
// is not used in this test, and the callback is treated as the hook
add_action("hook", "f");
add_action("hook", "g");
add_action("hook", "h");
/* 
 * pre-OAPI: Initiates a non-flattened call stack of internal->user_code
 * to ensure that cufa instrumentation properly handles skipping
 * opline lookups of internal functions
 *
 * OAPI: Initiates a call to an added action outside
 * the context of do_action, to ensure we only instrument
 * with an active hook
 */
$function = new ReflectionFunction('g');
$function->invoke();

do_action("f");
