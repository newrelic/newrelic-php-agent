/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * To the greatest degree possible this file should contain only the code
 * required to initialize the agent and register it with PHP.
 */
#include "php_agent.h"
#include "nr_version.h"
#include "php_newrelic.h"
#include "php_error.h"
#include "php_file_get_contents.h"
#include "php_api.h"
#include "php_api_internal.h"

extern PHP_FUNCTION(newrelic_curl_header_callback);

/*
 * Debugger support. Call this function from the debugger to get a
 * dump of the transaction.
 */
void nr_print_txn(FILE* fp) {
  nrtxn_t* txn = 0;

  TSRMLS_FETCH();
  txn = NRPRG(txn);

  if (0 == fp) {
    fp = stdout;
  }

  if (0 == txn) {
    fprintf(fp, "NO TXN!\n");
    fflush(fp);
    return;
  }

#define PRINT_STATUS(field, format) \
  fprintf(fp, " txn->status." #field "=" format "\n", txn->status.field);
  PRINT_STATUS(has_inbound_record_tt, "%d");
  PRINT_STATUS(has_outbound_record_tt, "%d");
  PRINT_STATUS(path_is_frozen, "%d");
  PRINT_STATUS(path_type, "%d");
  PRINT_STATUS(ignore, "%d");
  PRINT_STATUS(ignore_apdex, "%d");
  PRINT_STATUS(background, "%d");
  PRINT_STATUS(recording, "%d");
  PRINT_STATUS(rum_header, "%d");
  PRINT_STATUS(rum_footer, "%d");
  PRINT_STATUS(http_x_start, NR_TIME_FMT);
  PRINT_STATUS(cross_process, "%d");
#undef PRINT_STATUS

  fflush(fp);
}

/*
 * Debugger support. Call this function from the debugger to get a
 * snapshot printed to fp (defaults to stdout) of the NRPRG data.
 */
void nr_print_globals(FILE* fp) {
  TSRMLS_FETCH();

  if (0 == fp) {
    fp = stdout;
  }

  fprintf(fp, "attributes=%d\n", NRPRG(attributes).enabled.value);
  fprintf(fp, "transaction_tracer_attributes=%d\n",
          NRPRG(transaction_tracer_attributes).enabled.value);
  fprintf(fp, "error_collector_attributes=%d\n",
          NRPRG(error_collector_attributes).enabled.value);
  fprintf(fp, "transaction_events_attributes=%d\n",
          NRPRG(transaction_events_attributes).enabled.value);
  fprintf(fp, "span_events_attributes=%d\n",
          NRPRG(span_events_attributes).enabled.value);
  fprintf(fp, "browser_monitoring_attributes=%d\n",
          NRPRG(browser_monitoring_attributes).enabled.value);

  fprintf(fp, "tt_threshold_is_apdex_f=%d\n", NRPRG(tt_threshold_is_apdex_f));

  fprintf(fp, "current_framework=%d\n", (int)NRPRG(current_framework));
  fprintf(fp, "framework_version=%d\n", NRPRG(framework_version));

  fprintf(fp, "execute_count=%d\n", NRTXNGLOBAL(execute_count));
  fprintf(fp, "php_cur_stack_depth=%d\n", NRPRG(php_cur_stack_depth));

  fprintf(fp, "txn=%p\n", NRPRG(txn));

  fprintf(fp, "start_sample=" NR_TIME_FMT "\n", NRPRG(start_sample));

  fprintf(fp, "start_user_time=" NR_TIME_FMT ".%06d\n",
          (nrtime_t)NRPRG(start_user_time.tv_sec),
          (int)NRPRG(start_user_time.tv_usec));
  fprintf(fp, "start_sys_time=" NR_TIME_FMT ".%06d\n",
          (nrtime_t)NRPRG(start_sys_time.tv_sec),
          (int)NRPRG(start_user_time.tv_usec));

  fprintf(fp, "wtfuncs_where=%d\n", NRPRG(wtfuncs_where));
  fprintf(fp, "wtfiles_where=%d\n", NRPRG(wtfiles_where));
  fprintf(fp, "ttcustom_where=%d\n", NRPRG(ttcustom_where));

  fprintf(fp, "deprecated_capture_request_parameters=%d\n",
          NRPRG(deprecated_capture_request_parameters));

  fprintf(fp, "extensions=%p\n", NRPRG(extensions));
  fflush(fp);

  nr_print_txn(fp);
}

/*
 * New Relic API function argument descriptors.
 */

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
ZEND_BEGIN_ARG_INFO_EX(newrelic_arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()
#endif /* PHP 8.0+ */

ZEND_BEGIN_ARG_INFO_EX(newrelic_get_request_metadata_arginfo, 0, 0, 0) 
ZEND_ARG_INFO(0, transport) 
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_add_custom_parameter_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, parameter)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_custom_metric_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, metric)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_background_job_arginfo, 0, 0, 0)
ZEND_ARG_INFO(0, background)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_name_transaction_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_add_custom_tracer_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, functionname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_enable_params_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_capture_params_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_browser_timing_arginfo, 0, 0, 0)
ZEND_ARG_INFO(0, with_tags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_set_appname_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, appname)
ZEND_ARG_INFO(0, license_key)
ZEND_ARG_INFO(0, xmit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_start_transaction_arginfo, 0, 0, 0)
ZEND_ARG_INFO(0, appname)
ZEND_ARG_INFO(0, license_key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_set_user_attributes_arginfo, 0, 0, 3)
ZEND_ARG_INFO(0, user)
ZEND_ARG_INFO(0, account)
ZEND_ARG_INFO(0, product)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_end_transaction_arginfo, 0, 0, 0)
ZEND_ARG_INFO(0, ignore)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_record_custom_event_arginfo, 0, 0, 0)
ZEND_ARG_INFO(0, event_type)
ZEND_ARG_ARRAY_INFO(0, parameters, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_add_custom_span_parameter_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, key)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_record_datastore_segment_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, callback)
ZEND_ARG_ARRAY_INFO(0, parameters, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_accept_distributed_trace_headers_arginfo,
                       0,
                       0,
                       1)
ZEND_ARG_INFO(0, headers)
ZEND_ARG_INFO(0, transport_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_accept_distributed_trace_payload_arginfo,
                       0,
                       0,
                       1)
ZEND_ARG_INFO(0, payload)
ZEND_ARG_INFO(0, transport_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(
    newrelic_accept_distributed_trace_payload_httpsafe_arginfo,
    0,
    0,
    1)
ZEND_ARG_INFO(0, payload)
ZEND_ARG_INFO(0, transport_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_set_user_id_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, uuid)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(newrelic_set_error_group_callback_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

/*
 * New Relic Distributed Trace API
 */
ZEND_BEGIN_ARG_INFO_EX(newrelic_create_distributed_trace_payload_arginfo,
                       0,
                       0,
                       0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_insert_distributed_trace_headers_arginfo,
                       0,
                       0,
                       1)
ZEND_ARG_INFO(1, headers)
ZEND_END_ARG_INFO()

/*
 * Other New Relic Functions
 */
ZEND_BEGIN_ARG_INFO_EX(newrelic_curl_header_callback_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, curl_resource)
ZEND_ARG_INFO(0, header_data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_add_headers_to_context_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, stream_context)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_remove_headers_from_context_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, stream_context)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_exception_handler_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, exception)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_notice_error_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, exception)
ZEND_ARG_INFO(0, errstr)
ZEND_ARG_INFO(0, fname)
ZEND_ARG_INFO(0, line_nr)
ZEND_ARG_INFO(0, ctx)
ZEND_END_ARG_INFO()

/*
 * Integration test helpers
 */
#ifdef ENABLE_TESTING_API
ZEND_BEGIN_ARG_INFO_EX(newrelic_get_metric_table_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, scoped)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(newrelic_is_localhost_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, host)
ZEND_END_ARG_INFO()
#endif /* ENABLE_TESTING_API */

static zend_function_entry newrelic_functions[] = {

    // clang-format off
    /*
     * API Functions
     */
    PHP_FE(newrelic_end_transaction,newrelic_end_transaction_arginfo)
    PHP_FE(newrelic_start_transaction,newrelic_start_transaction_arginfo)
    PHP_FE(newrelic_background_job, newrelic_background_job_arginfo)
    PHP_FE(newrelic_add_custom_parameter,newrelic_add_custom_parameter_arginfo)
    PHP_FE(newrelic_name_transaction, newrelic_name_transaction_arginfo)
    PHP_FE(newrelic_add_custom_tracer,newrelic_add_custom_tracer_arginfo)
    PHP_FE(newrelic_custom_metric, newrelic_custom_metric_arginfo)
    PHP_FE(newrelic_capture_params,newrelic_capture_params_arginfo)
    PHP_FE(newrelic_enable_params,newrelic_enable_params_arginfo)
    PHP_FE(newrelic_get_browser_timing_header,newrelic_browser_timing_arginfo)
    PHP_FE(newrelic_get_browser_timing_footer,newrelic_browser_timing_arginfo)
    PHP_FE(newrelic_set_appname, newrelic_set_appname_arginfo)
    PHP_FE(newrelic_set_user_attributes, newrelic_set_user_attributes_arginfo)
    PHP_FE(newrelic_record_custom_event, newrelic_record_custom_event_arginfo)
    PHP_FE(newrelic_record_datastore_segment, newrelic_record_datastore_segment_arginfo)
    PHP_FE(newrelic_create_distributed_trace_payload, newrelic_create_distributed_trace_payload_arginfo)
    PHP_FE(newrelic_insert_distributed_trace_headers, newrelic_insert_distributed_trace_headers_arginfo)
    PHP_FE(newrelic_add_custom_span_parameter, newrelic_add_custom_span_parameter_arginfo)
    PHP_FE(newrelic_set_user_id, newrelic_set_user_id_arginfo)
    PHP_FE(newrelic_set_error_group_callback, newrelic_set_error_group_callback_arginfo)
    PHP_FE(newrelic_notice_error, newrelic_notice_error_arginfo)

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
    PHP_FE(newrelic_ignore_transaction, newrelic_arginfo_void)
    PHP_FE(newrelic_ignore_apdex, newrelic_arginfo_void)
    PHP_FE(newrelic_end_of_transaction, newrelic_arginfo_void)
    PHP_FE(newrelic_disable_autorum, newrelic_arginfo_void)
    PHP_FE(newrelic_is_sampled, newrelic_arginfo_void)
#else
    PHP_FE(newrelic_ignore_transaction, 0)
    PHP_FE(newrelic_ignore_apdex, 0)
    PHP_FE(newrelic_end_of_transaction,0)
    PHP_FE(newrelic_disable_autorum, 0)
    PHP_FE(newrelic_is_sampled, 0)
#endif /* PHP8+ */


    /*
    * Other Functions
    */
    PHP_FE(newrelic_curl_header_callback, newrelic_curl_header_callback_arginfo)
    PHP_FE(newrelic_add_headers_to_context, newrelic_add_headers_to_context_arginfo)
    PHP_FE(newrelic_remove_headers_from_context, newrelic_remove_headers_from_context_arginfo)
    PHP_FE(newrelic_exception_handler, newrelic_exception_handler_arginfo)
    PHP_FE(newrelic_accept_distributed_trace_headers, newrelic_accept_distributed_trace_headers_arginfo)
    PHP_FE(newrelic_accept_distributed_trace_payload, newrelic_accept_distributed_trace_payload_arginfo)
    PHP_FE(newrelic_accept_distributed_trace_payload_httpsafe, newrelic_accept_distributed_trace_payload_httpsafe_arginfo)
    PHP_FE(newrelic_get_request_metadata, newrelic_get_request_metadata_arginfo)

#ifdef PHP8
    PHP_FE(newrelic_get_linking_metadata, newrelic_arginfo_void)
    PHP_FE(newrelic_get_trace_metadata, newrelic_arginfo_void)
#else
    PHP_FE(newrelic_get_linking_metadata, 0)
    PHP_FE(newrelic_get_trace_metadata, 0)
#endif /* PHP 8 */
    /*
     * Integration test helpers
     */
#ifdef ENABLE_TESTING_API

    PHP_FE(newrelic_get_metric_table, newrelic_get_metric_table_arginfo)
    PHP_FE(newrelic_is_localhost, newrelic_is_localhost_arginfo)

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
    PHP_FE(newrelic_get_hostname, newrelic_arginfo_void)
    PHP_FE(newrelic_get_slowsqls, newrelic_arginfo_void)
    PHP_FE(newrelic_get_trace_json, newrelic_arginfo_void)
    PHP_FE(newrelic_get_error_json, newrelic_arginfo_void)
    PHP_FE(newrelic_get_transaction_guid, newrelic_arginfo_void)
    PHP_FE(newrelic_is_recording, newrelic_arginfo_void)
#else
    PHP_FE(newrelic_get_hostname, 0)
    PHP_FE(newrelic_get_slowsqls, 0)
    PHP_FE(newrelic_get_trace_json, 0)
    PHP_FE(newrelic_get_error_json, 0)
    PHP_FE(newrelic_get_transaction_guid, 0)
    PHP_FE(newrelic_is_recording, 0)
#endif /* PHP 8 */

#endif /* ENABLE_TESTING_API */

    {0, 0, 0, 0, 0}};

// clang-format on

zend_module_entry newrelic_module_entry
    = {STANDARD_MODULE_HEADER,
       PHP_NEWRELIC_EXT_NAME,
       newrelic_functions,
       PHP_MINIT(newrelic),
       PHP_MSHUTDOWN(newrelic),
       PHP_RINIT(newrelic),
       PHP_RSHUTDOWN(newrelic),
       PHP_MINFO(newrelic),
#ifdef NR_VERSION
       NR_VERSION, /* defined by the Makefile, see VERSION file */
#else
       "unreleased",
#endif
       PHP_MODULE_GLOBALS(newrelic),
       PHP_GINIT(newrelic),
       PHP_GSHUTDOWN(newrelic),
       nr_php_post_deactivate,
       STANDARD_MODULE_PROPERTIES_EX};

#ifdef COMPILE_DL_NEWRELIC
ZEND_DLEXPORT zend_module_entry* get_module(void);
ZEND_GET_MODULE(newrelic)
#endif
