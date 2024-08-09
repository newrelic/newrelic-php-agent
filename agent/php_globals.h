/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles global variables and state.
 */
#ifndef PHP_GLOBALS_HDR
#define PHP_GLOBALS_HDR

/*
 * Per-process globals. These are all stored in a single data structure rather
 * that having lots of external variables. This makes the namespace cleaner
 * if we are unable to limit symbol visibility, and abstracts the access of
 * these globals behind a macro in case some future environment ever needs to
 * deal with them differently. These are the agent-specific globals.
 */
typedef struct _nrphpglobals_t {
  int enabled;              /* Is the agent globally enabled? */
  int our_module_number;    /* Module number for our extension */
  int mpm_bad;              /* True if we're disabled due to the worker MPM */
  int cli;                  /* Set to 1 if this is a cli/cgi invocation */
  char* ssl_cafile;         /* Path to SSL CA bundle */
  char* ssl_capath;         /* Path to directory of SSL CA certs */
  char* collector;          /* Collector host */
  char* proxy;              /* Egress proxy */
  char* daemon;             /* Path to daemon executable */
  char* pidfile;            /* Path to PID file */
  char* daemon_logfile;     /* Daemon log file */
  char* daemon_loglevel;    /* Daemon log level */
  char* daemon_auditlog;    /* Daemon audit log file name (if any) */
  char* daemon_app_timeout; /* Daemon application inactivity timeout */
  nrtime_t
      daemon_app_connect_timeout; /* Daemon application connection timeout */
  char* daemon_start_timeout;     /* Daemon startup timeout */
  char* udspath;      /* Legacy path for daemon, set by newrelic.daemon.port */
  char* address_path; /* Path for daemon, set by newrelic.daemon.address */
  nr_conn_params_t* daemon_conn_params; /* Daemon connection information */
  char* php_version;                    /* PHP version number */
  nr_utilization_t utilization;         /* Various daemon utilization flags */
  int no_daemon_launch;            /* Prevent agent from launching daemon */
  int daemon_special_curl_verbose; /* Cause the daemon to enter curl verbose
                                      mode */
  int daemon_special_integration; /* Cause daemon to dump special log entries to
                                     help integration testing. */
  nrobj_t* metadata; /* P17 metadata taken from environment variables with the
                      * prefix `NEW_RELIC_METADATA_` */
  char* env_labels;  /* Labels taken from environment variables with the
                      * prefix `NEW_RELIC_LABEL_` and from the environment
                      * variable with the key `NEW_RELIC_LABELS`	 */
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP 8.1+ */
  zend_long zend_offset;                    /* Zend extension offset */
  zend_long
      zend_op_array_offset; /* Zend extension op_array to modify reserved */
#else
  int zend_offset;          /* Zend extension offset */
  int zend_op_array_offset; /* Zend extension op_array to modify reserved */
#endif
  int done_instrumentation;  /* Set to true if we have installed instrumentation
                                handlers */
  nrtime_t expensive_min;    /* newrelic.special.expensive_node_min */
  char* upgrade_license_key; /* License key from special file created during 2.9
                                upgrades */
  nrobj_t* appenv;           /* Application environment */
  int instrument_extensions; /* newrelic.special.enable_extension_instrumentation
                              */
  int instrument_internal; /* newrelic.transaction_tracer.internal_functions_enabled
                            */
  int high_security;       /* newrelic.high_security */

  int apache_major;    /* Apache major version */
  int apache_minor;    /* Apache minor version */
  int apache_patch;    /* Apache patch version */
  char* apache_add;    /* Additional Apache version information */
  int is_apache;       /* 1 if the process is Apache, 0 otherwise */
  int apache_threaded; /* 1 if a threaded MPM is in use, 0 otherwise */
  int preload_framework_library_detection; /* Enables preloading framework and
                                              library detection */
  char* docker_id; /* 64 byte hex docker ID parsed from /proc/self/mountinfo */
  int composer_exists; /* Check if composer exists*/

  /* Original PHP callback pointer contents */
  nrphperrfn_t orig_error_cb;
  nrphpexecfn_t orig_execute;
  nr_php_execute_internal_function_t orig_execute_internal;

  /* Original PHP SAPI header callback */
  nrphphdrfn_t orig_header_handler;

  struct {
    uint8_t no_sql_parsing;
    uint8_t show_sql_parsing;
    uint8_t enable_path_translated;
    uint8_t no_background_jobs;
    uint8_t show_executes;
    uint8_t show_execute_params;
    uint8_t show_execute_stack;
    uint8_t show_execute_returns;
    uint8_t show_executes_untrimmed;
    uint8_t no_exception_handler;
    uint8_t no_signal_handler;
    uint8_t debug_autorum;
    uint8_t show_loaded_files;
    uint8_t debug_cat;
    uint8_t debug_dt;
    uint8_t disable_laravel_queue;
  } special_flags; /* special control options */
} nrphpglobals_t;

extern nrphpglobals_t nr_php_per_process_globals;
#define NR_PHP_PROCESS_GLOBALS(X) nr_php_per_process_globals.X

/*
 * Purpose : Initialise the per-process global state of the PHP agent.
 *
 * Note    : This function clears the per-process globals by setting all fields
 *           to zero, but does not populate any of the fields. This is generally
 *           done in the agent's MINIT handler.
 */
extern void nr_php_global_init(void);

/*
 * Purpose : Destroys the per-process global state of the PHP agent.
 */
extern void nr_php_global_destroy(void);

typedef void (*nr_php_global_once_func_t)(void);

/*
 * Purpose : On the first call after nr_php_global_init(), the given function
 *           will be invoked. On subsequent calls, nothing will happen.
 *
 * Params  : 1. The callback to be invoked once per PHP engine.
 */
extern void nr_php_global_once(nr_php_global_once_func_t func);

#endif /* PHP_GLOBALS_HDR */
