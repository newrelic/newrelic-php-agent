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
newrelic.framework.wordpress.hooks.options = threshold
newrelic.framework.wordpress.hooks.threshold = 0
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT_METRICS_EXIST
Supportability/framework/WordPress/detected
WebTransaction/Action/page-template
Supportability/InstrumentedFunction/apply_filters
Supportability/InstrumentedFunction/do_action
Framework/WordPress/Hook/wp_loaded
Framework/WordPress/Hook/template_include
Framework/WordPress/Hook/wp_init
Framework/WordPress/Hook/the_content
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/InstrumentedFunction/add_filter
Framework/WordPress/Plugin/mock-plugin1
Framework/WordPress/Plugin/mock-plugin2
Framework/WordPress/Plugin/mock-theme1
Framework/WordPress/Plugin/mock-theme2
*/

/*EXPECT_ERROR_EVENTS null */

/* WordPress mock app */
require_once __DIR__.'/mock-wordpress-app.php';
