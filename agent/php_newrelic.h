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
#include "util_vector.h"

#define PHP_NEWRELIC_EXT_NAME "newrelic"
#define PHP_NEWRELIC_EXT_URL "https://newrelic.com/docs/php/new-relic-for-php"

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
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_execute_data *execute_data
#define NR_SPECIALFNPTR_ORIG_ARGS wraprec, auto_segment, execute_data
#define NR_SPECIALFN_PROTO nruserfn_t *wraprec, zend_execute_data *execute_data
#define NR_OP_ARRAY (&execute_data->func->op_array)
#define NR_EXECUTE_PROTO zend_execute_data* execute_data
#define NR_EXECUTE_ORIG_ARGS execute_data
#define NR_UNUSED_SPECIALFN (void)execute_data
#define NR_ZEND_EXECUTE_HOOK zend_execute_ex
#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_execute_data *execute_data
#define NR_SPECIALFNPTR_ORIG_ARGS wraprec, auto_segment, execute_data
#define NR_SPECIALFN_PROTO nruserfn_t *wraprec, zend_execute_data *execute_data
#define NR_OP_ARRAY (execute_data->op_array)
#define NR_EXECUTE_PROTO zend_execute_data* execute_data
#define NR_EXECUTE_ORIG_ARGS execute_data
#define NR_UNUSED_SPECIALFN (void)execute_data
#define NR_ZEND_EXECUTE_HOOK zend_execute_ex
#else /* PHP < 5.5 */
#define NR_SPECIALFNPTR_PROTO                              \
  struct _nruserfn_t *wraprec, nr_segment_t *auto_segment, \
      zend_op_array *op_array_arg
#define NR_SPECIALFNPTR_ORIG_ARGS wraprec, auto_segment, op_array_arg
#define NR_SPECIALFN_PROTO nruserfn_t *wraprec, zend_op_array *op_array_arg
#define NR_OP_ARRAY (op_array_arg)
#define NR_EXECUTE_PROTO zend_op_array* op_array_arg
#define NR_EXECUTE_ORIG_ARGS op_array_arg
#define NR_UNUSED_SPECIALFN (void)op_array_arg
#define NR_ZEND_EXECUTE_HOOK zend_execute
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

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
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
typedef void (*nrphpexecfn_t)(NR_EXECUTE_PROTO TSRMLS_DC);
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

nrinibool_t custom_events_enabled; /* newrelic.custom_insights_events.enabled */
nrinibool_t synthetics_enabled;    /* newrelic.synthetics.enabled */

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
    cross_process_enabled; /* newrelic.cross_application_tracer.enabled */

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

char* drupal_module_invoke_all_hook;      /* The current Drupal hook */
size_t drupal_module_invoke_all_hook_len; /* The length of the current Drupal
                                             hook */
size_t drupal_http_request_depth; /* The current depth of drupal_http_request()
                                     calls */

int symfony1_in_dispatch; /* Whether we are currently within a
                             sfFrontWebController::dispatch() frame */
int symfony1_in_error404; /* Whether we are currently within a
                             sfError404Exception::printStackTrace() frame */

char* wordpress_tag;                   /* The current WordPress tag */
nr_regex_t* wordpress_hook_regex;      /* Regex to sanitize hook names */
nr_regex_t* wordpress_plugin_regex;    /* Regex for plugin filenames */
nr_regex_t* wordpress_theme_regex;     /* Regex for theme filenames */
nr_regex_t* wordpress_core_regex;      /* Regex for plugin filenames */
nr_hashmap_t* wordpress_file_metadata; /* Metadata for plugin and theme names
                                          given a filename */

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
nriniuint_t max_span_events;     /* newrelic.special.max_span_events */
nrinistr_t
    trace_observer_host; /* newrelic.infinite_tracing.trace_observer.host */
nriniuint_t
    trace_observer_port; /* newrelic.infinite_tracing.trace_observer.port */
nriniuint_t
    span_queue_size; /* newrelic.infinite_tracing.span_events.queue_size */
nriniuint_t
    agent_span_queue_size; /* newrelic.infinite_tracing.span_events.agent_queue.size
                            */
nrinitime_t
    agent_span_queue_timeout; /* newrelic.infinite_tracing.span_events.agent_queue.timeout
                               */

/*
 * pid and user_function_wrappers are used to store user function wrappers.
 * Storing this on a request level (as opposed to storing it on transaction
 * level) is more robust when using multiple transactions in one request.
 */
uint64_t pid;
nr_vector_t* user_function_wrappers;

nrapp_t* app; /* The application used in the last attempt to initialize a
                 transaction */

nrtxn_t* txn; /* The all-important transaction pointer */

char* predis_ctx; /* The current Predis pipeline context name, if any */
nr_hashmap_t* predis_commands;

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
