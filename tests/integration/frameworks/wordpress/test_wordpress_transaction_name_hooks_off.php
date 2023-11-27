<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should name WordPress web transaction as an 'Action' with the
name generated from the template used to generate the page even when
WordPress hooks are disabled.
*/

/*SKIPIF*/

/*INI
newrelic.framework = wordpress
newrelic.framework.wordpress.hooks = false
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT_METRICS_EXIST
Supportability/InstrumentedFunction/apply_filters
WebTransaction/Action/template
*/

/*EXPECT_ERROR_EVENTS null */

// Mock WordPress hooks; only a single callback for a given hook can be defined
$wp_filters = [];

function add_filter($hook_name, $callback) {
  global $wp_filters;
  $wp_filters[$hook_name] = $callback;
}

function apply_filters($hook_name, $value, ...$args) {
  global $wp_filters;
  return call_user_func_array($wp_filters[$hook_name], array($value, $args));
}

// Hook 'template_include' with an identity filter 
function identity_filter($value) {
  return $value;
}
add_filter("template_include", "identity_filter");

// Emulate WordPress loading a template to render a page:
$template = apply_filters("template_include", "./path/to/templates/template.php");
