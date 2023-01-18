/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file declares API functions.
 */
#ifndef PHP_API_HDR
#define PHP_API_HDR

/*
 * Recommendations for API calls when using OAPI instrumentation and PHP 8+
 *
 * Dangling segments:
 * With the use of Observer API we have the possibility of dangling segments
 * that can occur due to an exception occurring.  In the normal course of
 * events, nr_php_observer_fcall_begin starts segments and
 * nr_php_observer_fcall_end keeps/discards/ends segments. However, in the case
 * of an uncaught exception, nr_php_observer_fcall_end is never called and
 * therefore, the logic to keep/discard/end the segment doesn't automatically
 * get initiated which can lead to dangling stacked segments.
 *
 * However, certain agent API calls need to be associated with particular
 * segments.
 *
 * To handle this , dangling exception cleanup is initiated by the following
 * call: nr_php_api_ensure_current_segment();
 *
 * ANY API call that depends on the current segment needs to use this function
 * to ensure the API uses the correct segment.
 */

extern void nr_php_api_add_supportability_metric(const char* name TSRMLS_DC);

extern void nr_php_api_error(const char* format, ...) NRPRINTFMT(1);

extern PHP_FUNCTION(newrelic_ignore_transaction);
extern PHP_FUNCTION(newrelic_ignore_apdex);
extern PHP_FUNCTION(newrelic_end_of_transaction);
extern PHP_FUNCTION(newrelic_end_transaction);
extern PHP_FUNCTION(newrelic_start_transaction);
extern PHP_FUNCTION(newrelic_background_job);
extern PHP_FUNCTION(newrelic_notice_error);
extern PHP_FUNCTION(newrelic_add_custom_parameter);
extern PHP_FUNCTION(newrelic_name_transaction);
extern PHP_FUNCTION(newrelic_add_custom_tracer);
extern PHP_FUNCTION(newrelic_custom_metric);
extern PHP_FUNCTION(newrelic_capture_params);
extern PHP_FUNCTION(newrelic_enable_params);
extern PHP_FUNCTION(newrelic_get_browser_timing_header);
extern PHP_FUNCTION(newrelic_get_browser_timing_footer);
extern PHP_FUNCTION(newrelic_disable_autorum);
extern PHP_FUNCTION(newrelic_set_appname);
extern PHP_FUNCTION(newrelic_set_user_attributes);
extern PHP_FUNCTION(newrelic_record_custom_event);
extern PHP_FUNCTION(newrelic_record_datastore_segment);
extern PHP_FUNCTION(newrelic_accept_distributed_trace_payload);
extern PHP_FUNCTION(newrelic_accept_distributed_trace_payload_httpsafe);
extern PHP_FUNCTION(newrelic_accept_distributed_trace_headers);
extern PHP_FUNCTION(newrelic_create_distributed_trace_payload);
extern PHP_FUNCTION(newrelic_insert_distributed_trace_headers);
extern PHP_FUNCTION(newrelic_get_linking_metadata);
extern PHP_FUNCTION(newrelic_get_trace_metadata);
extern PHP_FUNCTION(newrelic_is_sampled);
extern PHP_FUNCTION(newrelic_add_custom_span_parameter);

#endif /* PHP_API_HDR */
