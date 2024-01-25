<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test WordPress instrumentation when hooks and custom plugins are instrumented.
This setting is useful to observe custom plugins and themes performance.
The agent should:
 - detect WordPress framework
 - name the web transaction as an 'Action' named after the template used to generate the page
 - detect and report custom plugins and themes
 - generate hooks metrics with the execution time of callback functions from custom plugins
   and themes; execution of callback functions from wordpress core is _not_ captured.
No errors should be generated.
*/

/*SKIPIF
<?php
// This test checks if add_filter is instrumented and this function is only
// instrumented in PHPs 7.4 because it is used to wrap hook callback functions.
// Older PHPs use call_user_func_array instrumentation to wrap hook callbacks.
if (version_compare(PHP_VERSION, '7.4', '<')) {
    die("skip: PHP >= 7.4 required\n");
}
*/

/*INI
newrelic.framework.wordpress.hooks = true
newrelic.framework.wordpress.hooks.options = plugin_callbacks
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT_METRICS_EXIST
Supportability/framework/WordPress/detected
WebTransaction/Action/page-template
Supportability/InstrumentedFunction/apply_filters
Supportability/InstrumentedFunction/do_action
Supportability/InstrumentedFunction/add_filter
Framework/WordPress/Hook/wp_loaded
Framework/WordPress/Hook/template_include
Framework/WordPress/Plugin/mock-plugin1
Framework/WordPress/Plugin/mock-plugin2
Framework/WordPress/Plugin/mock-theme1
Framework/WordPress/Plugin/mock-theme2
*/

/*EXPECT_METRICS_DONT_EXIST
Framework/WordPress/Hook/wp_init
Framework/WordPress/Hook/the_content
*/

/*EXPECT_ERROR_EVENTS null */

/* WordPress mock app */
require_once __DIR__.'/mock-wordpress-app.php';
