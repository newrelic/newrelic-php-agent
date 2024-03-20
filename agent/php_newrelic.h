/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file handles agent initialization and registration.
 */
#ifndef PHP_NEWRELIC_HDR
#define PHP_NEWRELIC_HDR

#include "nr_banner.h"
#include "nr_mysqli_metadata.h"
#include "nr_segment.h"
#include "nr_txn.h"
#include "php_extension.h"
#include "util_hashmap.h"
#include "util_matcher.h"
#include "util_vector.h"

#define PHP_NEWRELIC_EXT_NAME "newrelic"

extern PHP_MINIT_FUNCTION(newrelic);
extern PHP_MSHUTDOWN_FUNCTION(newrelic);
extern PHP_RINIT_FUNCTION(newrelic);
extern PHP_RSHUTDOWN_FUNCTION(newrelic);
extern PHP_MINFO_FUNCTION(newrelic);
extern int nr_php_post_deactivate(void);

extern void nr_php_late_initialization(void);
extern nrobj_t* nr_php_app_settings(void);

extern zend_module_entry newrelic_module_entry;
#define phpext_newrelic_ptr &newrelic_module_entry

/*
 * PHP 5.5 and 7.0 both introduced a number of changes to the internal
 * representation of execute data state. These macros abstract away the vast
 * majority of those changes so the rest of the code can simply use the macros
 * and not have to take the different APIs into account.
 */

/* OVERWRITE_ZEND_EXECUTE_DATA allows testing of components with the previous
 * method of overwriting until the handler functions are complete.
 * Additionally, gives us flexibility of toggling back to previous method of
 * instrumentation. When checking in, leave this toggled on to have the CI work
 * as long as possible until the handler functionality is implemented.*/

//#define OVERWRITE_ZEND_EXECUTE_DATA true

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_execute_data *execute_data, zval *func_return_value
#define NR_SPECIALFNPTR_ORIG_ARGS \
  wraprec, auto_segment, execute_data, func_return_value
#define NR_SPECIALFN_PROTO                                \
  nruserfn_t *wraprec, zend_execute_data *execute_data, , \
      zval *func_return_value
#define NR_OP_ARRAY (&execute_data->func->op_array)
#define NR_EXECUTE_PROTO \
  zend_execute_data *execute_data, zval *func_return_value
#define NR_EXECUTE_PROTO_OVERWRITE zend_execute_data* execute_data
#define NR_EXECUTE_ORIG_ARGS_OVERWRITE execute_data
#define NR_EXECUTE_ORIG_ARGS execute_data, func_return_value
#define NR_UNUSED_SPECIALFN (void)execute_data
#define NR_UNUSED_FUNC_RETURN_VALUE (void)func_return_value
/* NR_ZEND_EXECUTE_HOOK to be removed in future ticket */
#define NR_ZEND_EXECUTE_HOOK zend_execute_ex

#elif ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ and overwrite hook*/
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_execute_data *execute_data
#define NR_SPECIALFNPTR_ORIG_ARGS wraprec, auto_segment, execute_data
#define NR_SPECIALFN_PROTO nruserfn_t *wraprec, zend_execute_data *execute_data
#define NR_OP_ARRAY (&execute_data->func->op_array)
#define NR_EXECUTE_PROTO zend_execute_data* execute_data
#define NR_EXECUTE_PROTO_OVERWRITE zend_execute_data* execute_data
#define NR_EXECUTE_ORIG_ARGS_OVERWRITE execute_data
#define NR_EXECUTE_ORIG_ARGS execute_data
#define NR_UNUSED_SPECIALFN (void)execute_data
#define NR_UNUSED_FUNC_RETURN_VALUE
#define NR_ZEND_EXECUTE_HOOK zend_execute_ex

#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_execute_data *execute_data
#define NR_SPECIALFNPTR_ORIG_ARGS wraprec, auto_segment, execute_data
#define NR_SPECIALFN_PROTO nruserfn_t *wraprec, zend_execute_data *execute_data
#define NR_OP_ARRAY (execute_data->op_array)
#define NR_EXECUTE_PROTO zend_execute_data* execute_data
#define NR_EXECUTE_PROTO_OVERWRITE zend_execute_data* execute_data
#define NR_EXECUTE_ORIG_ARGS_OVERWRITE execute_data
#define NR_EXECUTE_ORIG_ARGS execute_data
#define NR_UNUSED_SPECIALFN (void)execute_data
#define NR_UNUSED_FUNC_RETURN_VALUE
#define NR_ZEND_EXECUTE_HOOK zend_execute_ex

#else /* PHP < 5.5 */
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_op_array *op_array_arg
#define NR_SPECIALFNPTR_ORIG_ARGS wraprec, auto_segment, op_array_arg
#define NR_SPECIALFN_PROTO nruserfn_t *wraprec, zend_op_array *op_array_arg
#define NR_OP_ARRAY (op_array_arg)
#define NR_EXECUTE_PROTO zend_op_array* op_array_arg
#define NR_EXECUTE_PROTO_OVERWRITE zend_op_array* op_array_arg
#define NR_EXECUTE_ORIG_ARGS_OVERWRITE op_array_arg
#define NR_EXECUTE_ORIG_ARGS op_array_arg
#define NR_UNUSED_SPECIALFN (void)op_array_arg
#define NR_UNUSED_FUNC_RETURN_VALUE
#define NR_ZEND_EXECUTE_HOOK zend_execute
#endif

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
#define NR_GET_RETURN_VALUE_PTR func_return_value ? &func_return_value : NULL
#else
#define NR_GET_RETURN_VALUE_PTR nr_php_get_return_value_ptr(TSRMLS_C)
#endif

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
#define NR_UNUSED_EXECUTE_DATA (void)execute_data;
#define NR_UNUSED_HT
#define NR_UNUSED_RETURN_VALUE (void)return_value;
#define NR_UNUSED_RETURN_VALUE_PTR
#define NR_UNUSED_RETURN_VALUE_USED
#define NR_UNUSED_THIS_PTR
#else
#define NR_UNUSED_EXECUTE_DATA
#define NR_UNUSED_HT (void)ht;
#define NR_UNUSED_RETURN_VALUE (void)return_value;
#define NR_UNUSED_RETURN_VALUE_PTR (void)return_value_ptr;
#define NR_UNUSED_RETURN_VALUE_USED (void)return_value_used;
#define NR_UNUSED_THIS_PTR (void)this_ptr;
#endif /* PHP7+ */

/*
 * Convenience macro to handle unused TSRM parameters.
 */
#if ZTS && !defined(PHP7) && !defined(PHP8)
#define NR_UNUSED_TSRMLS (void)tsrm_ls;
#else
#define NR_UNUSED_TSRMLS
#endif

typedef enum {
  NR_FW_UNSET = 0,

  NR_FW_CAKEPHP,
  NR_FW_CODEIGNITER,
  NR_FW_DRUPAL, /* Drupal 6/7 */
  NR_FW_DRUPAL8,
  NR_FW_JOOMLA,
  NR_FW_KOHANA,
  NR_FW_LARAVEL,
  NR_FW_LUMEN,
  NR_FW_MAGENTO1,
  NR_FW_MAGENTO2,
  NR_FW_MEDIAWIKI,
  NR_FW_SILEX,
  NR_FW_SLIM,
  NR_FW_SYMFONY1,
  NR_FW_SYMFONY2,
  NR_FW_SYMFONY4,
  NR_FW_WORDPRESS,
  NR_FW_YII,
  NR_FW_ZEND,
  NR_FW_ZEND2,
  NR_FW_LAMINAS3,
  NR_FW_NONE, /* Must be immediately before NR_FW_MUST_BE_LAST */
  NR_FW_MUST_BE_LAST
} nrframework_t;

typedef struct _nrcallbackfn_t {
  zend_fcall_info fci;
  zend_fcall_info_cache fcc;
  bool is_set;
} nrcallbackfn_t;

/*
 * Per-request globals. This is designed for thread safety.
 * These are the globals that are accessible to each request, of which
 * there may be multiple in a multi-threaded environment. PHP takes care of
 * locking / multiple access via it's TSRM. Thus this is where we store any
 * data that is specific to each request, which is the vast majority of our
 * data. We have very few things that are genuinely global. Those few that are
 * are defined in php_agent.h.
 *
 * These are set by the PHP INI parser.
 * All variables that are set in the INI file actually use a small structure,
 * one that holds the value as well as where the value was set. Please see
 * php_nrini.c for details.
 */
typedef struct _nrinistr_t {
  char* value;
  int where;
} nrinistr_t;

typedef struct _nrinibool_t {
  zend_bool value;
  int where;
} nrinibool_t;

typedef struct _nriniuint_t {
  zend_uint value;
  int where;
} nriniuint_t;

typedef struct _nriniint_t {
  int value;
  int where;
} nriniint_t;

typedef struct _nrinitime_t {
  nrtime_t value;
  int where;
} nrinitime_t;

typedef struct _nrinifw_t {
  nrframework_t value;
  int where;
} nrinifw_t;

/*
 * Various function pointer types used for instrumentation and hook functions.
 */

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
typedef void (*nrphpfn_t)(INTERNAL_FUNCTION_PARAMETERS);
#else
typedef void(ZEND_FASTCALL* nrphpfn_t)(INTERNAL_FUNCTION_PARAMETERS);
#endif /* PHP < 7.3 */

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
typedef void (*nrphperrfn_t)(int type,
                             zend_string* error_filename,
                             uint error_lineno,
                             zend_string* message);
#elif ZEND_MODULE_API_NO == ZEND_8_0_X_API_NO
typedef void (*nrphperrfn_t)(int type,
                             const char* error_filename,
                             uint error_lineno,
                             zend_string* message);
#else
typedef void (*nrphperrfn_t)(int type,
                             const char* error_filename,
                             uint error_lineno,
                             const char* fmt,
                             va_list args)
    ZEND_ATTRIBUTE_PTR_FORMAT(printf, 4, 0);
#endif /* PHP >= 8.0 */
typedef zend_op_array* (*nrphpcfile_t)(zend_file_handle* file_handle,
                                       int type TSRMLS_DC);
typedef zend_op_array* (*nrphpcstr_t)(zval* source_string,
                                      char* filename TSRMLS_DC);
typedef void (*nrphpexecfn_t)(NR_EXECUTE_PROTO_OVERWRITE TSRMLS_DC);
typedef void (*nrphpcufafn_t)(zend_function* func,
                              const zend_function* caller TSRMLS_DC);
typedef int (*nrphphdrfn_t)(sapi_header_struct* sapi_header,
                            sapi_header_op_enum op,
                            sapi_headers_struct* sapi_headers TSRMLS_DC);

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
typedef void (*nr_php_execute_internal_function_t)(
    zend_execute_data* execute_data,
    zval* return_value);
#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
typedef void (*nr_php_execute_internal_function_t)(
    zend_execute_data* execute_data,
    zend_fcall_info* fci,
    int return_value_used TSRMLS_DC);
#else
typedef void (*nr_php_execute_internal_function_t)(
    zend_execute_data* execute_data,
    int return_value_used TSRMLS_DC);
#endif

typedef struct _nr_php_ini_attribute_config_t {
  nrinibool_t enabled;
  nrinistr_t include;
  nrinistr_t exclude;
} nr_php_ini_attribute_config_t;

/*
 * Globals
 */
ZEND_BEGIN_MODULE_GLOBALS(newrelic)
nrinistr_t license;  /* newrelic.license */
nrinistr_t appnames; /* newrelic.appname */
nrinibool_t enabled; /* newrelic.enabled */
nrinibool_t
    errors_enabled; /* newrelic.error_collector.enabled - paired with RPM */
nrinibool_t
    ignore_user_exception_handler; /* newrelic.error_collector.ignore_user_exception_handler
                                    */
nriniint_t ignore_errors;     /* newrelic.error_collector.ignore_errors */
nrinistr_t ignore_exceptions; /* newrelic.error_collector.ignore_exceptions */
nrinibool_t
    record_database_errors; /* newrelic.error_collector.record_database_errors
                             */
nrinibool_t
    prioritize_api_errors; /* newrelic.error_collector.prioritize_api_errors */
nrinibool_t
    remove_trailing_path; /* newrelic.webtransaction.name.remove_trailing_path
                           */

nrinibool_t
    browser_monitoring_auto_instrument; /* newrelic.browser_monitoring.auto_instrument
                                         */
nrinibool_t browser_monitoring_debug; /* newrelic.browser_monitoring.debug */
nrinistr_t browser_monitoring_loader; /* newrelic.browser_monitoring.loader */

nrinibool_t drupal_modules;  /* newrelic.framework.drupal.modules */
nrinibool_t wordpress_hooks; /* newrelic.framework.wordpress.hooks */
nrinistr_t
    wordpress_hooks_options; /* newrelic.framework.wordpress.hooks.options */
nrinitime_t wordpress_hooks_threshold; /* newrelic.framework.wordpress.hooks.threshold
                                        */
bool wordpress_plugins;                /* set based on
                                                 newrelic.framework.wordpress.hooks.options */
bool wordpress_core;                   /* set based on
                                                 newrelic.framework.wordpress.hooks.options */
nrinistr_t
    wordpress_hooks_skip_filename; /* newrelic.framework.wordpress.hooks_skip_filename
                                    */

nrinibool_t
    analytics_events_enabled; /* DEPRECATED newrelic.analytics_events.enabled */
nrinibool_t
    transaction_tracer_capture_attributes; /* DEPRECATED
                                              newrelic.transaction_tracer.capture_attributes
                                            */
nrinibool_t
    error_collector_capture_attributes; /* DEPRECATED
                                           newrelic.error_collector.capture_attributes
                                         */
nrinibool_t
    analytics_events_capture_attributes; /* DEPRECATED
                                            newrelic.analytics_events.capture_attributes
                                          */
nrinibool_t
    browser_monitoring_capture_attributes; /* DEPRECATED
                                              newrelic.browser_monitoring.capture_attributes
                                            */

nrinibool_t
    transaction_events_enabled;   /* newrelic.transaction_events.enabled */
nrinibool_t error_events_enabled; /* newrelic.error_collector.capture_events */

nr_php_ini_attribute_config_t attributes; /* newrelic.attributes.*  */
nr_php_ini_attribute_config_t
    transaction_tracer_attributes; /* newrelic.transaction_tracer.attributes.*
                                    */
nr_php_ini_attribute_config_t
    error_collector_attributes; /* newrelic.error_collector.attributes.*  */
nr_php_ini_attribute_config_t
    transaction_events_attributes; /* newrelic.transaction_events.attributes.*
                                    */
nr_php_ini_attribute_config_t
    span_events_attributes; /* newrelic.span_events.attributes.*/
nr_php_ini_attribute_config_t
    browser_monitoring_attributes; /* newrelic.browser_monitoring.attributes.*
                                    */
nr_php_ini_attribute_config_t
    log_context_data_attributes; /* newrelic.application_logging.forwarding.context_data.*
                                  */

nrinibool_t custom_events_enabled; /* newrelic.custom_insights_events.enabled */
nriniuint_t custom_events_max_samples_stored; /* newrelic.custom_events.max_samples_stored
                                               */
nrinibool_t synthetics_enabled;               /* newrelic.synthetics.enabled */

nrinibool_t phpunit_events_enabled; /* newrelic.phpunit_events.enabled */

nrinibool_t
    instance_reporting_enabled; /* newrelic.datastore_tracer.instance_reporting.enabled
                                 */
nrinibool_t
    database_name_reporting_enabled; /* newrelic.datastore_tracer.database_name_reporting.enabled
                                      */

/*
 * Deprecated settings that control request parameter capture.
 */
nrinibool_t capture_params; /* DEPRECATED newrelic.capture_params */
nrinistr_t ignored_params;  /* DEPRECATED newrelic.ignored_params */

nrinibool_t
    tt_enabled; /* newrelic.transaction_tracer.enabled - paired with RPM */
nrinibool_t ep_enabled; /* newrelic.transaction_tracer.explain_enabled */
nriniuint_t tt_detail;  /* newrelic.transaction_tracer.detail */
nriniuint_t
    tt_max_segments_web; /* newrelic.transaction_tracer.max_segments_web */
nriniuint_t
    tt_max_segments_cli; /* newrelic.transaction_tracer.max_segments_cli */
nrinibool_t tt_slowsql;  /* newrelic.transaction_tracer.slow_sql */
zend_bool tt_threshold_is_apdex_f; /* True if threshold is apdex_f */
nrinitime_t tt_threshold;          /* newrelic.transaction_tracer.threshold */
nrinitime_t ep_threshold; /* newrelic.transaction_tracer.explain_threshold */
nrinitime_t
    ss_threshold; /* newrelic.transaction_tracer.stack_trace_threshold */
nrinibool_t
    cross_process_enabled; /* DEPRECATED
                              newrelic.cross_application_tracer.enabled */

nriniuint_t max_nesting_level; /* newrelic.special.max_nesting_level (named
                                  after like-used variable in xdebug) */
nrinistr_t labels;             /* newrelic.labels */
nrinistr_t process_host_display_name; /* newrelic.process_host.display_name */
nrinistr_t file_name_list;            /* newrelic.webtransaction.name.files */
nrinibool_t
    tt_inputquery;        /* newrelic.transaction_tracer.gather_input_queries */
nriniuint_t tt_recordsql; /* newrelic.transaction_tracer.record_sql */
#define NR_PHP_RECORDSQL_OFF NR_SQL_NONE
#define NR_PHP_RECORDSQL_RAW NR_SQL_RAW
#define NR_PHP_RECORDSQL_OBFUSCATED NR_SQL_OBFUSCATED

nrinifw_t force_framework; /* newrelic.framework */
nrframework_t
    current_framework; /* Current request framework (forced or detected) */
int framework_version; /* Current framework version */

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
     && !defined OVERWRITE_ZEND_EXECUTE_DATA
/* Without OAPI, we are able to utilize the call stack to keep track
 * of the previous hooks. With OAPI, we can no longer do this so
 * we track the stack manually */
nr_stack_t drupal_invoke_all_hooks; /* stack of Drupal hooks */
nr_stack_t drupal_invoke_all_states; /* stack of bools indicating
                                               whether the current hook
                                               needs to be released */
#else
char* drupal_invoke_all_hook;      /* The current Drupal hook */
size_t drupal_invoke_all_hook_len; /* The length of the current Drupal
                                             hook */
#endif //OAPI
size_t drupal_http_request_depth; /* The current depth of drupal_http_request()
                                     calls */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
nr_segment_t* drupal_http_request_segment;
#endif
int symfony1_in_dispatch; /* Whether we are currently within a
                             sfFrontWebController::dispatch() frame */
int symfony1_in_error404; /* Whether we are currently within a
                             sfError404Exception::printStackTrace() frame */

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
     && !defined OVERWRITE_ZEND_EXECUTE_DATA
bool check_cufa;
/* Without OAPI, we are able to utilize the call stack to keep track
 * of the previous tags. With OAPI, we can no longer do this so
 * we track the stack manually */
nr_stack_t wordpress_tags;
nr_stack_t wordpress_tag_states; /* stack of bools indicating
                                    whether the current tag
                                    needs to be released */
#else
bool check_cufa; /* Whether we need to check cufa because we are
                    instrumenting hooks, or whether we can skip cufa */
char* wordpress_tag;                    /* The current WordPress tag */
#endif //OAPI

nr_matcher_t* wordpress_plugin_matcher; /* Matcher for plugin filenames */
nr_matcher_t* wordpress_theme_matcher;  /* Matcher for theme filenames */
nr_matcher_t* wordpress_core_matcher;   /* Matcher for plugin filenames */
nr_hashmap_t* wordpress_file_metadata;  /* Metadata for plugin and theme names
                                           given a filename */
nr_hashmap_t* wordpress_clean_tag_cache; /* Cached clean tags */                                           

char* doctrine_dql; /* The current Doctrine DQL. Only non-NULL while a Doctrine
                       object is on the stack. */

int php_cur_stack_depth; /* Total current depth of PHP stack, measured in PHP
                            call frames */

nrphpcufafn_t
    cufa_callback; /* The current call_user_func_array callback, if any */
/*
 * We instrument database connection constructors and store the instance
 * information in a hash keyed by a string containing the connection resource
 * id.

 * Some database extensions allow commands without explicit connections and
 * use the last known connection. <database>_last_conn tracks the hashmap key
 * for the last opened connection. Its presence can be used to determine
 * whether the last connection was valid.
 */
char* mysql_last_conn;
char* pgsql_last_conn;
nr_hashmap_t* datastore_connections;

nrinibool_t guzzle_enabled; /* newrelic.guzzle.enabled */

nrtime_t start_sample;          /* Time of starting rusage query */
struct timeval start_user_time; /* User rusage at transaction's start  */
struct timeval start_sys_time;  /* System rusage at transaction's start */

int wtfuncs_where;  /* Where was newrelic.webtransaction.name.functions set? */
int wtfiles_where;  /* Where was newrelic.webtransaction.name.files set? */
int ttcustom_where; /* Where was newrelic.transaction_tracer.custom set? */

/*
 * Request parameters are now controlled by attribute configuration.
 * However, for backwards compatibility, the capture of request parameters
 * can still be controlled by:
 *
 *   * API function newrelic_enable_params
 *   * API function newrelic_capture_params
 *   * INI setting newrelic.capture_params
 *
 * This value tracks those mechanisms.
 */
int deprecated_capture_request_parameters;
nr_php_extensions_t* extensions; /* Instrumented extensions */

/*
 * List of callback functions used to filter which exceptions caught
 * by the agent's last chance exception handler should recorded as
 * traced errors.
 */
zend_llist exception_filters;

/*
 * Save a valid pointer to the sapi_headers_struct for the current response.
 * This field is NULL until the agent detects that the runtime layout
 * sapi_globals_struct differs from compile time.
 */
sapi_headers_struct* sapi_headers;

nrinistr_t security_policies_token; /* newrelic.security_policies_token */
nrinibool_t
    allow_raw_exception_messages; /* newrelic.allow_raw_exception_messages */
nrinibool_t custom_parameters_enabled; /* newrelic.custom_parameters_enabled */
nrinibool_t
    distributed_tracing_enabled; /* newrelic.distributed_tracing_enabled */
nrinibool_t
    distributed_tracing_exclude_newrelic_header; /* newrelic.distributed_tracing_exclude_newrelic_header
                                                  */
nrinibool_t span_events_enabled; /* newrelic.span_events_enabled */
nriniuint_t
    span_events_max_samples_stored; /* newrelic.span_events.max_samples_stored
                                     */
nrinistr_t
    trace_observer_host; /* newrelic.infinite_tracing.trace_observer.host */
nriniuint_t
    trace_observer_port; /* newrelic.infinite_tracing.trace_observer.port */
nriniuint_t
    span_queue_size; /* newrelic.infinite_tracing.span_events.queue_size */
nriniuint_t
    agent_span_queue_size; /* newrelic.infinite_tracing.span_events.agent_queue.size*/
nrinitime_t
    agent_span_queue_timeout; /* newrelic.infinite_tracing.span_events.agent_queue.timeout
                               */

/*
 * configuration options for handling application logging
 */

nrinibool_t logging_enabled; /* newrelic.application_logging.enabled */
nrinibool_t
    log_decorating_enabled; /* newrelic.application_logging.local_decorating.enabled
                             */
nrinibool_t
    log_forwarding_enabled; /* newrelic.application_logging.forwarding.enabled
                             */
nriniuint_t
    log_events_max_samples_stored; /* newrelic.application_logging.forwarding.max_samples_stored
                                    */
nrinibool_t
    log_metrics_enabled; /* newrelic.application_logging.metrics.enabled */

nriniuint_t
    log_forwarding_log_level; /* newrelic.application_logging.forwarding.log_level
                               */

/*
 * Configuration option to toggle code level metrics collection.
 */
nrinibool_t
    code_level_metrics_enabled; /* newrelic.code_level_metrics.enabled */

/*
 * Configuration option to enable or disable package detection for vulnerability management
 */
nrinibool_t
    vulnerability_management_package_detection_enabled; /* newrelic.vulnerability_management.package_detection.enabled
                                                         */

#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
/*
 * pid and user_function_wrappers are used to store user function wrappers.
 * Storing this on a request level (as opposed to storing it on transaction
 * level) is more robust when using multiple transactions in one request.
 */
uint64_t pid;
nr_vector_t* user_function_wrappers;
#endif

nrapp_t* app; /* The application used in the last attempt to initialize a
                 transaction */

nrtxn_t* txn; /* The all-important transaction pointer */

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
nr_stack_t predis_ctxs; /* Without OAPI, we are able to utilize the call
                           stack to keep track of the current predis_ctx.
                           WIth OAPI, we must track this manually */
#else
char* predis_ctx; /* The current Predis pipeline context name, if any */
#endif
nr_hashmap_t* predis_commands;

nrcallbackfn_t error_group_user_callback; /* The user defined callback for
                                              error group naming */

/*
 * The globals below all refer to a transaction. Those globals contain
 * information related to the active transaction and, contrary to the globals
 * above, have to be reset for each transaction started during a request.
 */
struct {
  int execute_count; /* How many times nr_php_execute_enabled was called */
  int generating_explain_plan; /* Are we currently working on an explain plan?
                                */
  nr_hashmap_t* guzzle_objs; /* Guzzle request object storage: requests that are
                                currently in progress are stored here */
  nr_mysqli_metadata_t* mysqli_links; /* MySQLi link metadata storage */
  nr_hashmap_t* mysqli_queries;       /* MySQLi query metadata storage */
  nr_hashmap_t* pdo_link_options;     /* PDO link option storage */
  int curl_ignore_setopt; /* Non-zero to disable curl_setopt instrumentation */
  nr_hashmap_t* curl_metadata;       /* curl metadata storage */
  nr_hashmap_t* curl_multi_metadata; /* curl multi metadata storage */
  nr_hashmap_t* prepared_statements; /* Prepared statement storage */
} txn_globals;

ZEND_END_MODULE_GLOBALS(newrelic)

/*
 * Zend uses newrelic_globals as the auto-generated name for the per-request
 * globals and then uses the same name to pass the per-request globals
 * as a parameter to the GINIT and GSHUTDOWN functions.
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

ZEND_EXTERN_MODULE_GLOBALS(newrelic)

extern PHP_GINIT_FUNCTION(newrelic);
extern PHP_GSHUTDOWN_FUNCTION(newrelic);

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(ZTS)
#define NRPRG(X) TSRMG(newrelic_globals_id, zend_newrelic_globals*, X)
#define NRINI(X) TSRMG(newrelic_globals_id, zend_newrelic_globals*, X.value)
#else
#define NRPRG(X) (newrelic_globals.X)
#define NRINI(X) (newrelic_globals.X.value)
#endif

#define NRTXN(Y) (NRPRG(txn)->Y)
#define NRTXNGLOBAL(Y) (NRPRG(txn_globals).Y)

static inline int nr_php_recording(TSRMLS_D) {
  if (nrlikely((0 != NRPRG(txn)) && (0 != NRPRG(txn)->status.recording))) {
    return 1;
  } else {
    return 0;
  }
}

static inline bool is_error_callback_set() {
    return NRPRG(error_group_user_callback).is_set;
}

/*
 * Purpose : Print out the transaction. Call this function from the debugger.
 *
 * Params  : 1. The file handle to use.  0 defaults to stdout.
 */
extern void nr_print_txn(FILE* fp);

/*
 * Purpose : Print out global variables. Call this function from the debugger.
 *
 * Params  : 1. The file handle to use.  0 defaults to stdout.
 */
void nr_print_globals(FILE* fp);

/*
 * Purpose : Consults configuration settings and file system markers to decide
 *           if the agent should start the dameon.
 *           See the documentation in:
 *           https://docs.newrelic.com/docs/agents/php-agent/installation/starting-php-daemon
 *           https://docs.newrelic.com/docs/agents/php-agent/configuration/php-agent-configuration
 *
 * Returns : a nr_daemon_startup_mode_t describing the daemon startup mode.
 *
 * Note    : This function only returns valid values after the newrelic PHP
 *           module is initialized.
 */
nr_daemon_startup_mode_t nr_php_get_daemon_startup_mode(void);

#endif /* PHP_NEWRELIC_HDR */
