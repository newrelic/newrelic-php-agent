<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that the locale is handled correctly in w3c dt tracestate header
priority for locales that use `,` instead of `.` for decimal values
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = false
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT
ok - insert function succeeded
ok - preexisting header is present
ok - newrelic header is present
ok - tracestate header is present
ok - traceparent header is present
ok - locale handled correctly for w3c dt priority
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$headers = array('Accept-Language' => 'en-US,en;q=0.5');
setlocale(LC_NUMERIC, 'fr_FR');
tap_assert(newrelic_insert_distributed_trace_headers($headers), 'insert function succeeded');
tap_assert(array_key_exists('Accept-Language', $headers), 'preexisting header is present');
tap_assert(array_key_exists('newrelic', $headers), 'newrelic header is present');
tap_assert(array_key_exists('tracestate', $headers), 'tracestate header is present');
tap_assert(array_key_exists('traceparent', $headers), 'traceparent header is present');
tap_refute(strpos($headers['tracestate'], ','), 'locale handled correctly for w3c dt priority');

