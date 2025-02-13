/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains data types and functions for dealing with a transaction.
 *
 * It defines the data types and functions used to record a single transaction.
 * A transaction is defined as a single web request or a single invocation on
 * the command line. A transaction can also be started and stopped
 * programatically, by means of API calls.
 */
#ifndef NR_TXN_HDR
#define NR_TXN_HDR

#include <stdint.h>
#include <stdbool.h>

#include "nr_analytics_events.h"
#include "nr_app.h"
#include "nr_attributes.h"
#include "nr_errors.h"
#include "nr_file_naming.h"
#include "nr_log_events.h"
#include "nr_log_level.h"
#include "nr_segment.h"
#include "nr_slowsqls.h"
#include "nr_span_queue.h"
#include "nr_synthetics.h"
#include "nr_distributed_trace.h"
#include "nr_php_packages.h"
#include "util_apdex.h"
#include "util_buffer.h"
#include "util_hashmap.h"
#include "util_json.h"
#include "util_metrics.h"
#include "util_minmax_heap.h"
#include "util_sampling.h"
#include "util_slab.h"
#include "util_stack.h"
#include "util_string_pool.h"

#define NR_TXN_REQUEST_PARAMETER_ATTRIBUTE_PREFIX "request.parameters."
typedef enum _nr_tt_recordsql_t {
  NR_SQL_NONE = 0,
  NR_SQL_RAW = 1,
  NR_SQL_OBFUSCATED = 2
} nr_tt_recordsql_t;

/*
 * This structure contains transaction options.
 * Originally, this structure was populated at the transaction's start and
 * never modified:  If options needed to be changed, then a duplicate setting
 * would be put into the status structure.  This has been abandoned and
 * "autorum_enabled" and "request_params_enabled" may be changed during the
 * transaction.
 */
typedef struct _nrtxnopt_t {
  int custom_events_enabled; /* Whether or not to capture custom events */
  size_t custom_events_max_samples_stored; /* The maximum number of custom
                                              events per transaction */
  int synthetics_enabled; /* Whether or not to enable Synthetics support */
  int instance_reporting_enabled; /* Whether to capture datastore instance host
                                     and port */
  int database_name_reporting_enabled; /* Whether to include database name in
                                          datastore instance */
  int err_enabled;                     /* Whether error reporting is enabled */
  int request_params_enabled; /* Whether recording request parameters is enabled
                               */
  int autorum_enabled;        /* Whether auto-RUM is enabled or not */
  int analytics_events_enabled; /* Whether to record analytics events */
  int error_events_enabled;     /* Whether to record error events */
  int tt_enabled;               /* Whether to record TT's or not */
  int ep_enabled;               /* Whether to request explain plans or not */
  nr_tt_recordsql_t
      tt_recordsql; /* How to record SQL statements in TT's (if at all) */
  int tt_slowsql;   /* Whether to support the slow SQL feature */
  nrtime_t apdex_t; /* From app default unless key txn */
  nrtime_t
      tt_threshold;  /* TT threshold in usec - faster than this isn't a TT */
  int tt_is_apdex_f; /* tt_threshold is 4 * apdex_t */
  nrtime_t ep_threshold;     /* Explain Plan threshold in usec */
  nrtime_t ss_threshold;     /* Slow SQL stack threshold in usec */
  int cross_process_enabled; /* DEPRECATED Whether or not to read and modify
                                headers */
  int allow_raw_exception_messages; /* Whether to replace the error/exception
                                       messages with generic text */
  int custom_parameters_enabled;    /* Whether to allow recording of custom
                                       parameters/attributes */

  int distributed_tracing_enabled; /* Whether distributed tracing functionality
                                      is enabled */
  bool distributed_tracing_pad_trace_id; /* whether to pad internally generated
                                            trace_id to NR_TRACE_ID_MAX_SIZE
                                            characters */
  bool distributed_tracing_exclude_newrelic_header; /* Whether distributed
                                                       tracing outbound headers
                                                       should omit newrelic
                                                       headers in favor of only
                                                       W3C trace context headers
                                                     */
  int span_events_enabled; /* Whether span events are enabled */
  size_t
      span_events_max_samples_stored; /* The maximum number of span events per
                                         transaction. When set to 0, the default
                                         event limit
                                         NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED
                                         is used. */
  size_t max_segments; /* The maximum number of segments that are kept in the
                          segment tree at a time. When set to 0 or 1, no maximum
                          is applied. */
  bool discount_main_context_blocking; /* If enabled, the main context is
                                          assumed to be blocked when
                                          asynchronous contexts are executing,
                                          and the total time is adjusted
                                          accordingly. */
  size_t span_queue_batch_size; /* The number of span events to batch in the
                                   queue before transmitting them to the daemon
                                   for on-transmission to 8T. When set to 0, no
                                   spans will be batched, and non-8T behaviour
                                   will be used. */
  nrtime_t span_queue_batch_timeout; /* Span queue batch timeout in us. */
  bool logging_enabled; /* An overall configuration for enabling/disabling all
                           application logging features */
  bool log_decorating_enabled; /* Whether log decorating is enabled */
  bool log_forwarding_enabled; /* Whether log forwarding is enabled */
  bool log_forwarding_context_data_enabled; /* Whether context data is forwarded
                                               with logs */
  int log_forwarding_log_level; /* minimum log level to forward to the collector
                                 */
  size_t log_events_max_samples_stored; /* The maximum number of log events per
                                           transaction */
  bool log_metrics_enabled;             /* Whether log metrics are enabled */
  bool log_forwarding_labels_enabled;   /* Whether labels are forwarded with log
                                           events */
  bool message_tracer_segment_parameters_enabled; /* Determines whether to add
                                                     message attr */
} nrtxnopt_t;

typedef enum _nrtxnstatus_cross_process_t {
  NR_STATUS_CROSS_PROCESS_DISABLED = 0, /* Cross process has been disabled */
  NR_STATUS_CROSS_PROCESS_START
  = 1, /* The response header has not been created */
  NR_STATUS_CROSS_PROCESS_RESPONSE_CREATED
  = 2 /* The response header has been created */
} nrtxnstatus_cross_process_t;

/*
 * There is precedence scheme to web transaction names. Larger numbers
 * indicate higher priority. Frozen paths are indicated with a
 * separate field in the txn structure; you should always consult the
 * path_is_frozen before doing other comparisons or assignments.
 */
typedef enum _nr_path_type_t {
  NR_PATH_TYPE_UNKNOWN = 0,
  NR_PATH_TYPE_URI = 1,
  NR_PATH_TYPE_STATUS_CODE = 2,
  NR_PATH_TYPE_ACTION = 3,
  NR_PATH_TYPE_FUNCTION = 4,
  NR_PATH_TYPE_CUSTOM = 5
} nr_path_type_t;

typedef struct _nrtxncat_t {
  char* inbound_guid;
  char* trip_id;
  char* referring_path_hash;
  nrobj_t* alternate_path_hashes;
  char* client_cross_process_id; /* Inbound X-NewRelic-ID (decoded and valid) */
} nrtxncat_t;

typedef struct _nrtxnstatus_t {
  int has_inbound_record_tt;  /* 1 if the inbound request header has a true
                                 record_tt, 0 otherwise */
  int has_outbound_record_tt; /* 1 if an outbound response header has a true
                                 record_tt, 0 otherwise */
  int path_is_frozen;         /* 1 is path is frozen, 0 otherwise */
  nr_path_type_t path_type;   /* Path type */
  int ignore;                 /* Set if this transaction should be ignored */
  int ignore_apdex; /* Set if no apdex metrics should be generated for this txn
                     */
  int background;   /* Set if this is a background job */
  int recording;    /* Set to 1 if we are recording, 0 if not */
  bool complete;  /* Set to true if the transaction is complete; false otherwise
                   */
  int rum_header; /* 0 = header not sent, 1 = sent manually, 2 = auto */
  int rum_footer; /* 0 = footer not sent, 1 = sent manually, 2 = auto */
  nrtime_t http_x_start; /* X-Request-Start time, or 0 if none */
  nrtxnstatus_cross_process_t cross_process;
} nrtxnstatus_t;

/*
 * Data products generated at the end of a transaction.
 */
typedef struct _nrtxnfinal_t {
  char* trace_json;
  nr_vector_t* span_events;
  nrtime_t total_time;
} nrtxnfinal_t;

/*
 * Members of this enumeration are used as an index into an array.
 */
typedef enum _nr_cpu_usage_t {
  NR_CPU_USAGE_START = 0,
  NR_CPU_USAGE_END = 1,
  NR_CPU_USAGE_COUNT = 2
} nr_cpu_usage_t;

typedef struct _nr_composer_info_t {
  bool autoload_detected;
  bool composer_detected;
} nr_composer_info_t;

/*
 * Possible transaction types, which go into the type bitfield in the nrtxn_t
 * struct.
 *
 * NR_TXN_TYPE_CAT_INBOUND indicates both X-NewRelic-ID header and a valid
 * X-NewRelic-Transaction header were received.
 *
 * NR_TXN_TYPE_CAT_OUTBOUND indicates that we sent one or more external
 * requests with CAT headers.
 *
 * NR_TXN_TYPE_DT_INBOUND indicates that an inbound DT payload was received.
 * X-NewRelic-Transaction header were received.
 *
 * NR_TXN_TYPE_DT_OUTBOUND indicates that we sent one or more external
 * requests with a DT payload.
 */
#define NR_TXN_TYPE_SYNTHETICS (1 << 0)
#define NR_TXN_TYPE_CAT_INBOUND (1 << 2)
#define NR_TXN_TYPE_CAT_OUTBOUND (1 << 3)
#define NR_TXN_TYPE_DT_INBOUND (1 << 4)
#define NR_TXN_TYPE_DT_OUTBOUND (1 << 5)
typedef uint32_t nrtxntype_t;

/*
 * The main transaction structure
 */
typedef struct _nrtxn_t {
  char* agent_run_id;   /* The agent run ID */
  int high_security;    /* From application: Whether the txn is in special high
                           security mode */
  int lasp;             /* From application: Whether the txn is in special lasp
                           enabled mode */
  nrtxnopt_t options;   /* Options for this transaction */
  nrtxnstatus_t status; /* Status for the transaction */
  nrtxncat_t cat;       /* Incoming CAT fields */
  nr_random_t* rnd;     /* Random number generator, owned by the application. */

  nr_stack_t default_parent_stack; /* A stack to track the current parent in a
                                      tree of segments, for segments that are
                                      not on an async context */
  nr_hashmap_t* parent_stacks;     /* A hashmap of stacks to track the current
                                      parent in a tree of segments, keyed by async
                                      context */
  nr_segment_t* force_current_segment; /* Enforce a current segment for the
                                          default context, overriding the
                                          default parent stack. */
  size_t segment_count; /* A count of segments for this transaction, maintained
                           throughout the life of this transaction */
  nr_minmax_heap_t*
      segment_heap; /* The heap used to track segments when a limit has been
                       applied via the max_segments transaction option. */
  nr_slab_t* segment_slab;    /* The slab allocator used to allocate segments */
  nr_segment_t* segment_root; /* The root pointer to the tree of segments */
  nrtime_t abs_start_time; /* The absolute start timestamp for this transaction;
                            * all segment start and end times are relative to
                            * this field */

  nr_error_t* error;            /* Captured error */
  nr_slowsqls_t* slowsqls;      /* Slow SQL statements */
  nrpool_t* datastore_products; /* Datastore products seen */
  nrpool_t* trace_strings;      /* String pool for transaction trace */
  nrmtable_t*
      scoped_metrics; /* Contains metrics that are both scoped and unscoped. */
  nrmtable_t* unscoped_metrics; /* Unscoped metric table for the txn */
  nrobj_t* intrinsics; /* Attribute-like builtin fields sent along with traces
                          and errors */
  nr_attribute_config_t*
      attribute_config; /* The attribute config for the transaction. This will
                           be used for enable attribute filtering on
                           segments/spans. */
  nr_attributes_t* attributes; /* Key+value pair tags put in txn event, txn
                                  trace, error, and browser */
  nr_file_naming_t* match_filenames; /* Filenames to match on for txn naming */

  nr_analytics_events_t*
      custom_events;               /* Custom events created through the API. */
  nr_log_events_t* log_events;     /* Log events pool */
  nrobj_t* log_forwarding_labels;  /* A hash of log labels to be added to log
                                      events */
  nr_php_packages_t* php_packages; /* Detected php packages */
  nr_php_packages_t*
      php_package_major_version_metrics_suggestions; /* Suggested packages for
                                  major metric creation */
  nrtime_t user_cpu[NR_CPU_USAGE_COUNT]; /* User CPU usage */
  nrtime_t sys_cpu[NR_CPU_USAGE_COUNT];  /* System CPU usage */

  char* license;     /* License copied from application for RUM encoding use. */
  char* request_uri; /* Request URI */
  char*
      path; /* Request URI or action (txn name before rules applied & prefix) */
  char* name; /* Full transaction metric name */

  nrtxntype_t type; /* The transaction type(s), as a bitfield */

  nrobj_t* app_connect_reply; /* Contents of application collector connect
                                 command reply */
  nr_app_limits_t app_limits; /* Application data limits */
  char* primary_app_name; /* The primary app name in use (ie the first rollup
                             entry) */
  nr_synthetics_t* synthetics; /* Synthetics metadata for the transaction */

  nr_distributed_trace_t*
      distributed_trace; /* distributed tracing metadata for the transaction */
  nr_span_queue_t* span_queue; /* span queue when 8T is enabled */
  nr_composer_info_t composer_info;

  /*
   * flag to indicate if one time (per transaction) logging metrics
   * have been created
   */
  bool created_logging_onetime_metrics;

  /*
   * Special control variables derived from named bits in
   * nrphpglobals_t.special_flags These are used to debug the agent, possibly in
   * the field.
   */
  struct {
    uint8_t no_sql_parsing;   /* don't do SQL parsing */
    uint8_t show_sql_parsing; /* show various steps in SQL feature parsing */
    uint8_t debug_cat;        /* extra logging for CAT */
    uint8_t debug_dt;         /* extra logging for DT */
  } special_flags;

  /*
   * Data products created in nr_txn_end() that are used when transmitting the
   * transaction.
   */
  nrtxnfinal_t final_data;
} nrtxn_t;

static inline int nr_txn_recording(const nrtxn_t* txn) {
  if (nrlikely((0 != txn) && (0 != txn->status.recording))) {
    return 1;
  } else {
    return 0;
  }
}

/*
 * Purpose : Compare two structs of type nrtxnopt_t.
 *
 * Params  : 1. A pointer to the first struct for comparison.
 *           2. A pointer to the second struct for comparison.
 *
 * Returns : true if
 *           - both options are equal,
 *           - all fields of the two options are equal,
 *           and false otherwise.
 *
 * Notes   : Defined for testing purposes, to test whether a generated
 *           set of options are initialized as expected.
 */
bool nr_txn_cmp_options(nrtxnopt_t* o1, nrtxnopt_t* o2);

/*
 * Purpose : Compare connect_reply and security_policies to settings found
 *           in opts. If SSC or LASP policies are more secure, update local
 *           settings to match and log a verbose debug message.
 *
 * Params  : 1. options set in the transaction
 *           2. connect_reply originally obtained from the daemon
 *           3. security_policies originally obtained from the daemon
 *
 */
void nr_txn_enforce_security_settings(nrtxnopt_t* opts,
                                      const nrobj_t* connect_reply,
                                      const nrobj_t* security_policies);

/*
 * Purpose : Start a new transaction belonging to the given application.
 *
 * Params  : 1. The relevant application.  This application is assumed
 *              to be locked and is not unlocked by this function.
 *           2. Pointer to the starting options for the transaction.
 *
 * Returns : A newly created transaction pointer or NULL if the request could
 *           not be completed.
 */
extern nrtxn_t* nr_txn_begin(nrapp_t* app,
                             const nrtxnopt_t* opts,
                             const nr_attribute_config_t* attribute_config,
                             const nrobj_t* log_forwarding_labels);

/*
 * Purpose : End a transaction by finalizing all metrics and timers.
 *
 * Params  : 1. Pointer to the transaction being ended.
 */
extern void nr_txn_end(nrtxn_t* txn);

/*
 * Purpose : Set the timing of a transaction.
 *
 *           1. The pointer to the transaction to be retimed.
 *           2. The new start time for the transaction, in microseconds since
 *              the since the UNIX epoch.
 *           3. The new duration for the transaction, in microseconds.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_txn_set_timing(nrtxn_t* txn, nrtime_t start, nrtime_t duration);

/*
 * Purpose : Set the transaction path and type.  Writes a log message.
 *
 * Params  : 1. A descriptive name for the originator of the txn name, used for
 *              logging.
 *           2. The transaction to set the path in.
 *           3. The path.
 *           4. The path type (nr_path_type_t above).
 *           5. Describes if it is OK to overwrite the existing transaction name
 *              at the same priority level.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Notes   : If the path type has already been frozen then this function
 *           silently ignores the request to change the path type.
 */
typedef enum _txn_assignment_t {
  NR_NOT_OK_TO_OVERWRITE,
  NR_OK_TO_OVERWRITE
} nr_txn_assignment_t;

extern nr_status_t nr_txn_set_path(const char* whence,
                                   nrtxn_t* txn,
                                   const char* path,
                                   nr_path_type_t ptype,
                                   nr_txn_assignment_t ok_to_override);

/*
 * Purpose : Set the request URI ("real path") for the transaction.
 *
 * Params  : 1. The transaction to set the path in.
 *           2. The request URI.
 *
 * Notes   : The request URI is used in transaction traces, slow sqls, and
 *           errors.  This function will obey the transaction's
 *           options.request_params_enabled
 *           setting and remove trailing '?' parameters correctly.
 */
extern void nr_txn_set_request_uri(nrtxn_t* txn, const char* uri);

/*
 * Purpose : Indicate whether or not an error with the given priority level
 *           would be saved in the transaction. Used to prevent gathering of
 *           information about errors that would not be saved.
 *
 * Params  : 1. The transaction pointer.
 *           2. The priority of the error. A higher number indicates a more
 *              serious error.
 *
 * Returns : NR_SUCCESS if the error would be saved, NR_FAILURE otherwise.
 */
extern nr_status_t nr_txn_record_error_worthy(const nrtxn_t* txn, int priority);

/*
 * Purpose : Record the given error in the transaction.
 *
 * Params  : 1. The transaction pointer.
 *           2. The priority of the error. A higher number indicates a more
 *              serious error.
 *           3. Whether to add the error to the current segment.
 *           4. The error message.
 *           5. The error class.
 *           6. Stack trace in JSON format.
 *
 * Returns : Nothing.
 *
 * Notes   : This function will still record an error when high security is
 *           enabled but the message will be replaced with a placeholder.
 */
#define NR_TXN_HIGH_SECURITY_ERROR_MESSAGE \
  "Message removed by New Relic high_security setting"

#define NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE \
  "Message removed by New Relic security settings"

extern void nr_txn_record_error(nrtxn_t* txn,
                                int priority,
                                bool add_to_segment,
                                const char* errmsg,
                                const char* errclass,
                                const char* stacktrace_json);

/*
 * Purpose : Create the transaction name, apply all rules to it, and store it
 *           in the transaction's string pool. It can later be used in the
 *           RUM buffer and for metrics. The transaction name is used to check
 *           if the transaction is a key transaction, and if so, the apdex value
 *           is updated. In the course of applying url_rules and txn_rules, if
 *           an 'ignore' rule is matched then the entire transaction should be
 *           ignored.
 *
 * Params  : 1. The transaction pointer.
 *
 * Returns : NR_FAILURE if the transaction should be ignored, NR_SUCCESS
 *           otherwise.
 *
 */
extern nr_status_t nr_txn_freeze_name_update_apdex(nrtxn_t* txn);

/*
 * Purpose : Create a supportability metric name to be created when the
 *           instrumented function is called.
 */
extern char* nr_txn_create_fn_supportability_metric(const char* function_name,
                                                    const char* class_name);

/*
 * Purpose : Force an unscoped metric with a single count of the given name.
 */
extern void nr_txn_force_single_count(nrtxn_t* txn, const char* metric_name);

/*
 * Purpose : Determine whether the given transaction trace should be
 *           force persisted when sent to the collector.  Force persisted
 *           traces should have some noteworthy property and should be
 *           sent to the collector with the "force_persist" JSON boolean
 *           set to true.
 *
 * Returns : 1 if the transaction should be force persisted, and 0 otherwise.
 */
extern int nr_txn_should_force_persist(const nrtxn_t* txn);

/*
 * Purpose : Destroy a transaction, freeing all of its associated memory.
 */
extern void nr_txn_destroy(nrtxn_t** txnptr);

/*
 * Purpose : Mark the transaction as being a background job or web transaction.
 *
 * Params  : 1. The current transaction.
 *           2. An optional string used in a debug log message to indicate
 *              why the background status of the transaction has been changed.
 */
extern void nr_txn_set_as_background_job(nrtxn_t* txn, const char* reason);
extern void nr_txn_set_as_web_transaction(nrtxn_t* txn, const char* reason);

/*
 * Purpose : Set the http response code of the transaction.
 */
extern void nr_txn_set_http_status(nrtxn_t* txn, int http_code);

/*
 * Purpose : Add a key:value attribute pair to the current transaction's
 *           custom parameters.
 *
 * Returns : NR_SUCCESS if the parameter was added successfully, or NR_FAILURE
 *           if there was any sort of error.
 */
extern nr_status_t nr_txn_add_user_custom_parameter(nrtxn_t* txn,
                                                    const char* key,
                                                    const nrobj_t* value);

/*
 * Purpose : Add a request parameter to the transaction's attributes.
 *
 * Params  : 1. The current transaction.
 *           2. The request parameter name.
 *           3. The request parameter value.
 *           4. Whether or not request parameters have been enabled by a
 *              deprecated (non-attribute) configuration setting.
 */
extern void nr_txn_add_request_parameter(nrtxn_t* txn,
                                         const char* key,
                                         const char* value,
                                         int legacy_enable);

/*
 * Purpose : These attributes have special functions since the request referrer
 *           must be cleaned and the content length is converted to a string.
 */
extern void nr_txn_set_request_referer(nrtxn_t* txn,
                                       const char* request_referer);
extern void nr_txn_set_request_content_length(nrtxn_t* txn,
                                              const char* content_length);

struct _nr_txn_attribute_t;
typedef struct _nr_txn_attribute_t nr_txn_attribute_t;

/*
 * Purpose : Create attributes with the correct names and destinations.
 *           For relevant links see the comment above the definitions.
 */
extern const nr_txn_attribute_t* nr_txn_request_uri;
extern const nr_txn_attribute_t* nr_txn_request_accept_header;
extern const nr_txn_attribute_t* nr_txn_request_content_type;
extern const nr_txn_attribute_t* nr_txn_request_content_length;
extern const nr_txn_attribute_t* nr_txn_request_host;
extern const nr_txn_attribute_t* nr_txn_request_method;
extern const nr_txn_attribute_t* nr_txn_request_user_agent_deprecated;
extern const nr_txn_attribute_t* nr_txn_request_user_agent;
extern const nr_txn_attribute_t* nr_txn_server_name;
extern const nr_txn_attribute_t* nr_txn_response_content_type;
extern const nr_txn_attribute_t* nr_txn_response_content_length;
extern void nr_txn_set_string_attribute(nrtxn_t* txn,
                                        const nr_txn_attribute_t* attribute,
                                        const char* value);
extern void nr_txn_set_long_attribute(nrtxn_t* txn,
                                      const nr_txn_attribute_t* attribute,
                                      long value);
/*
 * Purpose : Return the duration of the transaction.  This function will return
 *           0 if the transaction has not yet finished or if the transaction
 *           is NULL.
 */
extern nrtime_t nr_txn_duration(const nrtxn_t* txn);

/*
 * Purpose : Return the duration of the transaction up to now.
 *           This function returns now - txn's start time.
 */
extern nrtime_t nr_txn_unfinished_duration(const nrtxn_t* txn);

/*
 * Purpose : Return the queue time associated with this transaction.
 *           If no queue start time has been recorded then this function
 *           will return 0.
 */
extern nrtime_t nr_txn_queue_time(const nrtxn_t* txn);

/*
 * Purpose : Set the time at which this transaction entered a web server queue
 *           prior to being started.
 */
extern void nr_txn_set_queue_start(nrtxn_t* txn, const char* x_request_start);

/*
 * Purpose : Add a custom event.
 */
extern void nr_txn_record_custom_event(nrtxn_t* txn,
                                       const char* type,
                                       const nrobj_t* params);

/*
 * Purpose : Check log forwarding configuration
 */
extern bool nr_txn_log_forwarding_enabled(nrtxn_t* txn);

/*
 * Purpose : Check log forwarding context data configuration
 */
extern bool nr_txn_log_forwarding_context_data_enabled(nrtxn_t* txn);

/*
 * Purpose : Check log forwarding log level configuration
 */
extern bool nr_txn_log_forwarding_log_level_verify(nrtxn_t* txn,
                                                   const char* log_level_name);

/*
 * Purpose : Check logging metrics configuration
 */
extern bool nr_txn_log_metrics_enabled(nrtxn_t* txn);

/*
 * Purpose : Check log decorating configuration
 */
extern bool nr_txn_log_decorating_enabled(nrtxn_t* txn);

/*
 * Purpose : Add a log event to transaction
 *
 * Params  : 1. The transaction.
 *           2. Log record level name
 *           3. Log record message
 *           4. Log record timestamp
 *           5. Attribute data for Monolog context data (can be NULL)
 *           6. The application (to get linking meta data)
 *
 */
extern void nr_txn_record_log_event(nrtxn_t* txn,
                                    const char* level_name,
                                    const char* message,
                                    nrtime_t timestamp,
                                    nr_attributes_t* context_attributes,
                                    nrapp_t* app);

/*
 * Purpose : Check log labels forwarding configuration
 */
extern bool nr_txn_log_forwarding_labels_enabled(nrtxn_t* txn);

/*
 * Purpose : Return the CAT trip ID for the current transaction.
 *
 * Params  : 1. The current transaction.
 *
 * Returns : A pointer to the current trip ID within the transaction struct.
 */
extern const char* nr_txn_get_cat_trip_id(const nrtxn_t* txn);

/*
 * Purpose : Return the GUID for the given transaction.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A pointer to the current GUID within the transaction struct.
 */
extern const char* nr_txn_get_guid(const nrtxn_t* txn);

/*
 * Purpose : Generate and return the current CAT path hash for the transaction.
 *
 * Params  : 1. The current transaction.
 *
 * Returns : A newly allocated string containing the hash, or NULL if an error
 *           occurred.
 */
extern char* nr_txn_get_path_hash(nrtxn_t* txn);

/*
 * Purpose : Checks if the given account ID is a trusted account for CAT.
 *
 * Params  : 1. The current transaction.
 *           2. The account ID to check.
 *
 * Returns : Non-zero if the account is trusted; zero otherwise.
 */
extern int nr_txn_is_account_trusted(const nrtxn_t* txn, int account_id);

/*
 * Purpose : Checks if the given account ID is a trusted account for DT.
 *
 * Params  : 1. The current transaction.
 *           2. The account ID to check.
 *
 * Returns : true if the account is trusted; false otherwise.
 */
extern bool nr_txn_is_account_trusted_dt(const nrtxn_t* txn,
                                         const char* trusted_key);

/*
 * Purpose : Check if the transaction should create apdex metrics.
 *
 * Params  : 1. The transaction.
 *
 * Returns : Non-zero if the transaction should create apdex metrics; zero
 *           otherwise.
 */
extern int nr_txn_should_create_apdex_metrics(const nrtxn_t* txn);

/*
 * Purpose : Checks if a transaction trace should be saved for this
 *           transaction.
 *
 * Params  : 1. The transaction.
 *           2. The duration of the transaction.
 *
 * Returns : Non-zero if a trace should be saved; zero otherwise.
 */
extern int nr_txn_should_save_trace(const nrtxn_t* txn, nrtime_t duration);

/*
 * Purpose : Return 1 if the txn's nr.guid should be added as an
 *           intrinsic to the txn's analytics event, and 0 otherwise.
 */
extern int nr_txn_event_should_add_guid(const nrtxn_t* txn);

/*
 * Purpose : Get the effective SQL recording setting for the transaction, taking
 *           into account high security mode.
 *
 * Params  : 1. The transaction.
 *
 * Returns : The recording level.
 */
extern nr_tt_recordsql_t nr_txn_sql_recording_level(const nrtxn_t* txn);

/*
 * Purpose : Returns whether or not the transaction is being sampled in a
 *           distributed tracing context.  Returns false if distributed
 *           tracing is disabled.
 *
 * Params  : 1. The transaction.
 *
 * Returns : true if distributed tracing is enabled and the transaction is
 *           sampled, false if the transaction is not sanpled or distributed
 *           tracing is disabled
 */
extern bool nr_txn_is_sampled(const nrtxn_t* txn);

/*
 * Purpose : Adds CAT intrinsics to the analytics event parameters.
 */
extern void nr_txn_add_cat_analytics_intrinsics(const nrtxn_t* txn,
                                                nrobj_t* intrinsics);

/*
 * Purpose : Generate the apdex zone for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. The duration of the transaction.
 *
 * Returns : The apdex.
 */
extern nr_apdex_zone_t nr_txn_apdex_zone(const nrtxn_t* txn, nrtime_t duration);

extern int nr_txn_is_synthetics(const nrtxn_t* txn);

/*
 * Purpose : Returns the time at which the txn started as a double. Returns 0
 *           if the txn is NULL.
 */
extern double nr_txn_start_time_secs(const nrtxn_t* txn);

/*
 * Purpose : Returns the time at which the txn started as an nrtime_t. Returns 0
 *           if the txn is NULL.
 */
extern nrtime_t nr_txn_start_time(const nrtxn_t* txn);

/*
 * Purpose : Given a transaction and a time relative to the start of the
 *           transaction, return the absolute time.
 *
 * Params  : 1. The transaction.
 *           2. A relative time.
 *
 * Returns : An absolute time. relative_time if the txn is NULL.
 */
static inline nrtime_t nr_txn_time_rel_to_abs(const nrtxn_t* txn,
                                              const nrtime_t relative_time) {
  if (nrunlikely(NULL == txn)) {
    return relative_time;
  }
  return txn->abs_start_time + relative_time;
}

/*
 * Purpose : Given a transaction and an absolute time, return the time
 *           relative to the start of the transaction.
 *
 * Params  : 1. The transaction.
 *           2. An absolute time.
 *
 * Returns : A time relative to the transaction. absolute_time if the txn is
 *           NULL.
 */
static inline nrtime_t nr_txn_time_abs_to_rel(const nrtxn_t* txn,
                                              const nrtime_t absolute_time) {
  if (nrunlikely(NULL == txn)) {
    return absolute_time;
  }
  return nr_time_duration(txn->abs_start_time, absolute_time);
}

/*
 * Purpose : Return the current relative time for a transaction.
 *
 * Params  : 1. The transaction.
 *
 * Returns : The relative time for this very moment in a transaction.
 */
inline static nrtime_t nr_txn_now_rel(const nrtxn_t* txn) {
  if (nrunlikely(NULL == txn)) {
    return 0;
  }
  return nr_time_duration(txn->abs_start_time, nr_get_time());
}

/*
 * Purpose : Add a comma-separated list of regex patterns to be matched against
 *           for file naming to a transaction.
 *
 * Params  : 1. The transaction.
 *           2. A string containing a comma-separated list of regex patterns.
 */
extern void nr_txn_add_match_files(nrtxn_t* txn,
                                   const char* comma_separated_list);

/*
 * Purpose : Check a filename against the list of match patterns registered for
 *           a given transaction. If a match is found, name the transaction
 *           according to the txn config.
 *
 * Params  : 1. The transaction.
 *           2. A file name.
 */
extern void nr_txn_match_file(nrtxn_t* txn, const char* filename);

/*
 * Purpose : Generate an error event.
 *
 * Params  : 1. The transaction.
 *
 * Returns : An error event.
 */
extern nr_analytics_event_t* nr_error_to_event(const nrtxn_t* txn);

/*
 * Purpose : Generate a transaction event.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A transaction event.
 */
extern nr_analytics_event_t* nr_txn_to_event(const nrtxn_t* txn);

/*
 * Purpose : Name the transaction from a function which has been specified by
 *           the user to be the name of the transaction if called.
 */
extern void nr_txn_name_from_function(nrtxn_t* txn,
                                      const char* funcname,
                                      const char* classname);

/*
 * Purpose : Ignore the current transaction and stop recording.
 *
 * Params  : 1. The transaction.
 *
 * Returns : true if the transaction could be ignore, and false otherwise.
 */
extern bool nr_txn_ignore(nrtxn_t* txn);

/*
 * Purpose : Add a custom metric from the API.
 *
 * Params  : 1. The transaction.
 *           2. The metric name.
 *           3. The metric duration.
 *
 * Returns : NR_SUCCESS if the metric could be added, and NR_FAILURE otherwise.
 *
 * NOTE    : No attempt is made to vet the metric name choice. The name could
 *           collide with any New Relic metric name.
 */
extern nr_status_t nr_txn_add_custom_metric(nrtxn_t* txn,
                                            const char* name,
                                            double value_ms);

/*
 * Purpose : Checks if the transaction name matches a string
 *
 * Params  : 1. The transaction.
 *           2. The string to match.
 *
 * Returns : true if the string matches, false otherwise.
 */
extern bool nr_txn_is_current_path_named(const nrtxn_t* txn, const char* name);

/*
 * Purpose : Accept a distributed trace header. This will attempt to use W3C
 *           style headers, if the traceparent is missing it will fall back and
 *           attempt to use a New Relic header.
 *
 * Params  : 1. The transaction.
 *           2. A hashmap of headers
 *           3. The type of transporation (e.g. "http", "https")
 *
 * Returns : true if we're able to accept a header, false otherwise.
 */
bool nr_txn_accept_distributed_trace_payload_httpsafe(
    nrtxn_t* txn,
    nr_hashmap_t* header_map,
    const char* transport_type);

/*
 * Purpose : Accept a distributed tracing payload for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. A hashmap of headers
 *           3. The type of transporation (e.g. "http", "https")
 *
 * Returns : true if we're able to accept the payload, false otherwise.
 */
bool nr_txn_accept_distributed_trace_payload(nrtxn_t* txn,
                                             nr_hashmap_t* header_map,
                                             const char* transport_type);

/*
 * Purpose : Create a distributed tracing payload for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. The segment to create the payload on.
 *
 * Returns : A newly allocated, null terminated payload string, which the caller
 *           must destroy with nr_free() when no longer needed, or NULL on
 *           error.
 *
 * Note    : The segment parameter must not be NULL: callers may wish to use
 *           nr_txn_get_current_segment() to get the current segment on the
 *           context they are interested in if a segment isn't explicitly
 *           available.
 */
extern char* nr_txn_create_distributed_trace_payload(nrtxn_t* txn,
                                                     nr_segment_t* segment);

/*
 * Purpose : Verify settings and create a W3C traceparent header.
 *
 * Params : 1. The current transaction.
 *          2. The current segment.
 *
 * Returns : A W3C traceparent header. Returns NULL on error.
 */
char* nr_txn_create_w3c_traceparent_header(nrtxn_t* txn, nr_segment_t* segment);

/*
 * Purpose : Create a W3C tracestate header.
 *
 * Params : 1. The transaction.
 *          2. The segment to create the payload on.
 *
 * Returns : A W3C tracestate header. Returns NULL on error.
 */
char* nr_txn_create_w3c_tracestate_header(const nrtxn_t* txn,
                                          nr_segment_t* segment);

/*
 * Purpose : Determine whether span events should be created. This is true if
 *           `distributed_tracing_enabled` and `span_events_enabled` are true
 *           and the transaction is sampled.
 *
 * Params  : 1. The transaction.
 *
 * Returns : true if span events should be created.
 */
extern bool nr_txn_should_create_span_events(const nrtxn_t* txn);

/*
 * Purpose : Get a pointer to the currently-executing segment for a given
 *           async context.
 *
 * Params  : 1. The transaction.
 *           2. The async context, or NULL for the main context.
 *
 * Note    : In some cases, the parent of a segment shall be supplied, as in
 *           cases of newrelic_start_segment(). In other cases, the parent of
 *           a segment shall be determined by the currently executing segment.
 *           For the second scenario, get a pointer to the active segment so
 *           that a new, parented segment may be added to the transaction.
 *
 * Returns : A pointer to the active segment.
 */
extern nr_segment_t* nr_txn_get_current_segment(nrtxn_t* txn,
                                                const char* async_context);

/*
 * Purpose : Force the given segment to be the current segment.
 *
 * Params  : 1. The transaction.
 *           2. A segment. NULL to restore default behavior.
 *
 * Note    : This forces the given segment to be the current segment for the
 *           default context. The default parent stack is bypassed. This has the
 *           effect that the given segment will be used as parent for all
 *           segments subsequently started with nr_segment_start.
 *
 *           This function is useful to temporarily inject segments that don't
 *           use the default allocator.
 */
inline static void nr_txn_force_current_segment(nrtxn_t* txn,
                                                nr_segment_t* segment) {
  if (nrlikely(txn)) {
    txn->force_current_segment = segment;
  }
}

/*
 * Purpose : Set the current segment for the transaction.
 *
 * Params  : 1. The current transaction.
 *           2. A pointer to the currently-executing segment.
 *
 * Note    : On the transaction is a data structure used to manage the parenting
 *           of stacks for all async contexts. Currently it's implemented as a
 *           hashmap of stacks.  This call is equivalent to pushing a segment
 *           pointer onto the stack of parents for the relevant async context.
 *
 */
extern void nr_txn_set_current_segment(nrtxn_t* txn, nr_segment_t* segment);

/*
 * Purpose : Retire the given segment if it is the currently executing segment
 *           on its async context.
 *
 *           If the given segment is not the currently executing segment on its
 *           async context, this function will do nothing.
 *
 * Params  : The current transaction.
 *
 * Note    : On the transaction is a data structure used to manage the parenting
 *           of stacks for all async contexts. Currently it's implemented as a
 *           hashmap of stacks.  This call is equivalent to popping a segment
 *           pointer from the stack of parents for the relevant async context.
 */
extern void nr_txn_retire_current_segment(nrtxn_t* txn, nr_segment_t* segment);

/*
 * Purpose : Destroy the fields within an nrtxnfinal_t.
 *
 * Params  : 1. A pointer to the nrtxnfinal_t to destroy.
 */
extern void nr_txn_final_destroy_fields(nrtxnfinal_t* tf);

/*
 * Purpose : Return the trace ID for the given transaction.
 *
 * Params  : 1. The transaction.
 *
 * Note    : The string returned has to be freed by the caller.
 *
 * Returns : Trace ID if distributed tracing is enabled, otherwise NULL.
 */
extern char* nr_txn_get_current_trace_id(nrtxn_t* txn);

/*
 * Purpose : Return the current span ID or create it if doesn't have one yet.
 *
 * Params  : 1. The transaction.
 *
 * Note    : The string returned has to be freed by the caller.
 *
 * Returns : current span ID if the segment is valid, otherwise NULL.
 */
extern char* nr_txn_get_current_span_id(nrtxn_t* txn);

/*
 * Purpose : End all currently active segments.
 *
 *           All segments in the parent stacks maintained by the transaction
 *           will be ended and removed from the parent stacks.
 *
 * Params  : 1. The transaction.
 *
 * Note    : This function should not be used when manual segment parenting and
 *           timing was used.
 */
extern void nr_txn_finalize_parent_stacks(nrtxn_t* txn);

/*
 * Purpose : Returns the number of segments allocated for this transaction.
 *
 *           This number is the number of segments obtained by the slab
 *           allocator, not the number of segments actually allocated by the
 *           slab allocator.
 *
 * Params  : 1. The transaction.
 *
 * Returns : The number of segments allocated for the given transaction.
 */
static inline size_t nr_txn_allocated_segment_count(const nrtxn_t* txn) {
  if (txn) {
    return nr_slab_count(txn->segment_slab);
  } else {
    return 0;
  }
}

/*
 * Purpose : Allocate a new segment.
 *
 *           All segments in the parent stacks maintained by the transaction
 *           will be ended and removed from the parent stacks.
 *
 * Params  : 1. The transaction.
 *
 * Returns : An uninitialized segment. The segment has yet to be initialized
 *           with nr_segment_init.
 */
static inline nr_segment_t* nr_txn_allocate_segment(nrtxn_t* txn) {
  if (NULL == txn) {
    return NULL;
  } else {
    return nr_slab_next(txn->segment_slab);
  }
}

/*
 * Purpose : Add php package to transaction from desired source. This function
 * should only be called when Vulnerability Management is enabled.
 *
 * Params  : 1. The transaction
 *           2. Package name
 *           3. Package version
 *           4. Source priority
 *
 * Returns : pointer to added package on success or NULL otherwise.
 */
nr_php_package_t* nr_txn_add_php_package_from_source(
    nrtxn_t* txn,
    char* package_name,
    char* package_version,
    const nr_php_package_source_priority_t source);

/*
 * Purpose : Add php package to transaction from legacy source. This function
 * should only be called when Vulnerability Management is enabled.
 *
 * Params  : 1. The transaction
 *           2. Package name
 *           3. Package version
 *
 * Returns : pointer to added package on success or NULL otherwise.
 */
extern nr_php_package_t* nr_txn_add_php_package(nrtxn_t* txn,
                                                char* package_name,
                                                char* package_version);

/*
 * Purpose : Add php package suggestion to transaction. This function
 * can be used when Vulnerability Management is not enabled.  It will
 * add the package to the transaction's
 * php_package_major_version_metrics_suggestions list. At the end of the
 * transaction this list is traversed and any suggestions with a known version
 * will have a package major version metric created.
 *
 * Params  : 1. The transaction
 *           2. Package name
 *           3. Package version (can be NULL or PHP_PACKAGE_VERSION_UNKNOWN)
 *
 * Returns : Nothing.
 */
extern void nr_txn_suggest_package_supportability_metric(
    nrtxn_t* txn,
    const char* package_name,
    const char* package_version);
#endif /* NR_TXN_HDR */
