<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_get_metric_table() should return segment and transaction metrics.
 */

/*EXPECT
ok - scoped metrics is an array
ok - scoped segment metric created
ok - unscoped metrics is an array
ok - custom metric created
*/

require_once(realpath(dirname( __FILE__ )).'/../../../include/tap.php');

/* Create some metrics */

newrelic_add_custom_tracer("should_be_in_metrics");

function should_be_in_metrics() {
   newrelic_custom_metric('Custom/Application/Metric', 1e+3);
}

should_be_in_metrics();

/* Take metric tables and re-key them by metric name */

$raw_metrics_scoped = newrelic_get_metric_table(true);
$raw_metrics_unscoped = newrelic_get_metric_table(false);

$metrics_scoped = array();
$metrics_unscoped = array();

array_walk(
    $raw_metrics_scoped, 
    function (stdClass $metric) use (&$metrics_scoped) {
	$metrics_scoped[$metric->name] = $metric;
});

array_walk(
    $raw_metrics_unscoped, 
    function (stdClass $metric) use (&$metrics_unscoped) {
	$metrics_unscoped[$metric->name] = $metric;
});

/* Test for metrics */

tap_assert(
    is_array($metrics_scoped), 
    'scoped metrics is an array');
tap_assert(
    array_key_exists('Custom/should_be_in_metrics', $metrics_scoped), 
    'scoped segment metric created');

tap_assert(
    is_array($metrics_unscoped), 
    'unscoped metrics is an array');
tap_assert(
    array_key_exists('Custom/Application/Metric', $metrics_unscoped), 
    'custom metric created');
