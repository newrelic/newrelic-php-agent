<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test WordPress instrumentation when hooks are instrumented but custom plugins are not.
This setting is not very useful unless wordpress core performance is to be observed.
The agent should:
 - detect WordPress framework
 - name the web transaction as an 'Action' named after the template used to generate the page
 - generate hooks metrics for all callback functions executions that last longer than
   threshold duration; the execution time of callback functions from wordpress core,
   custom plugins and themes is captured.
The agent should not:
 - detect and report custom plugins and themes
No errors should be generated.
*/

/*INI
newrelic.framework.wordpress.hooks = true
newrelic.framework.wordpress.plugins = false
newrelic.framework.wordpress.hooks_threshold = 500ms
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT_METRICS_EXIST
Supportability/framework/WordPress/detected
WebTransaction/Action/page-template
Supportability/InstrumentedFunction/apply_filters
Supportability/InstrumentedFunction/do_action
Framework/WordPress/Hook/hook_with_slow_callbacks
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/InstrumentedFunction/add_filter
Framework/WordPress/Plugin/mock-plugin1
Framework/WordPress/Plugin/mock-plugin2
Framework/WordPress/Plugin/mock-theme1
Framework/WordPress/Plugin/mock-theme2
Framework/WordPress/Hook/wp_loaded
Framework/WordPress/Hook/template_include
Framework/WordPress/Hook/wp_init
Framework/WordPress/Hook/the_content
Framework/WordPress/Hook/hook_with_fast_callbacks
*/

/*EXPECT_ERROR_EVENTS null */

/* WordPress mock app */
require_once __DIR__.'/mock-wordpress-app.php';

function wp_core_slow_action_1(...$args) {
  usleep(1000000); // must be greater than newrelic.framework.wordpress.hooks_threshold
  return;
}

function wp_core_fast_action_1(...$args) {
  usleep(10000); // must be less than newrelic.framework.wordpress.hooks_threshold
  return;
}

add_action("hook_with_slow_callbacks", "wp_core_slow_action_1");
add_action("hook_with_fast_callbacks", "wp_core_fast_action_1");

// Execute hook with slow callbacks:
do_action("hook_with_slow_callbacks");

// Execute hook with fast callbacks:
do_action("hook_with_fast_callbacks");
