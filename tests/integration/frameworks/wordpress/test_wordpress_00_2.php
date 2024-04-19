<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that agent falls back to WordPress default instrumentation when the settings in INI are invalid.
The agent should:
 - detect WordPress framework
 - name the web transaction as an 'Action' named after the template used to generate the page
 - detect custom plugins and themes
 - generate hooks metrics only for plugins and themes callback functions executions; 
   only the execution time of callback functions from custom plugins and themes is captured.
No errors should be generated.
*/

/*INI
newrelic.framework.wordpress.hooks.options="foobar"
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
