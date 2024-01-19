<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test WordPress instrumentation when hooks are not instrumented. The agent should:
 - detect WordPress framework
 - name the web transaction as an 'Action' named after the template used to generate the page
The agent should not:
 - detect custom plugins and themes
 - generate hooks metrics
No errors should be generated.
*/

/*INI
newrelic.framework.wordpress.hooks = false
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT_METRICS_EXIST
Supportability/framework/WordPress/detected
WebTransaction/Action/page-template
Supportability/InstrumentedFunction/apply_filters
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/InstrumentedFunction/do_action
Supportability/InstrumentedFunction/add_filter
Framework/WordPress/Hook/wp_loaded
Framework/WordPress/Hook/template_include
Framework/WordPress/Plugin/mock-plugin1
Framework/WordPress/Plugin/mock-plugin2
Framework/WordPress/Plugin/mock-theme1
Framework/WordPress/Plugin/mock-theme2
Framework/WordPress/Hook/wp_init
Framework/WordPress/Hook/the_content
*/

/*EXPECT_ERROR_EVENTS null */

/* WordPress mock app */
require_once __DIR__.'/mock-wordpress-app.php';
