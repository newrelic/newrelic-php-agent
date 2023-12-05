/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles the initialization that happens once per module load.
 */
#include "php_agent.h"

#include <dlfcn.h>
#include <signal.h>

#include "php_api_distributed_trace.h"
#include "php_environment.h"
#include "php_error.h"
#include "php_extension.h"
#include "php_globals.h"
#include "php_header.h"
#include "php_hooks.h"
#include "php_internal_instrument.h"
#include "php_samplers.h"
#include "php_user_instrument.h"
#include "php_vm.h"
#include "php_wrapper.h"
#include "fw_laravel.h"
#include "fw_wordpress.h"
#include "lib_guzzle4.h"
#include "lib_guzzle6.h"
#include "nr_agent.h"
#include "nr_app.h"
#include "nr_banner.h"
#include "nr_daemon_spawn.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_signals.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_threads.h"

static void php_newrelic_init_globals(zend_newrelic_globals* nrg) {
  if (nrunlikely(NULL == nrg)) {
    return;
  }

  nr_memset(nrg, 0, sizeof(*nrg));
  nrg->enabled.value = 1;
  nrg->enabled.where = PHP_INI_STAGE_STARTUP;
  nrg->current_framework = NR_FW_UNSET;
}

/*
 * Purpose : The customer-facing configurations newrelic.daemon.port and
 *           newrelic.daemon.address are aliases of each other.  However
 *           both cannot be set simultaneously.  This function examines
 *           whether each of these values has been set and returns a
 *           pointer to the daemon's address path. If both have been set,
 *           it returns a pointer to the string supplied by
 *           newrelic.daemon.address. If neither value has been set, a
 *           pointer to the default daemon location is returned.
 *
 * Returns : A pointer to the daemon's address path. The setting
 *           newrelic.daemon.address takes precedence over
 *           newrelic.daemon.port.
 */
static char* php_newrelic_init_daemon_path(void) {
  int port_is_set = nr_php_ini_setting_is_set_by_user("newrelic.daemon.port");
  int address_is_set
      = nr_php_ini_setting_is_set_by_user("newrelic.daemon.address");

  /* newrelic.daemon.address takes precedence */
  if (port_is_set && address_is_set) {
    nrl_warning(
        NRL_INIT,
        "Both newrelic.daemon.address and newrelic.daemon.port are set. "
        "Using newrelic.daemon.address: %s",
        NR_PHP_PROCESS_GLOBALS(address_path));
    return NR_PHP_PROCESS_GLOBALS(address_path);
  }
  if (port_is_set) {
    return NR_PHP_PROCESS_GLOBALS(udspath);
  } else if (address_is_set) {
    return NR_PHP_PROCESS_GLOBALS(address_path);
  }
  return NR_PHP_INI_DEFAULT_PORT;
}

/*
 * Zend uses newrelic_globals as the auto-generated name for the per-request
 * globals and then uses the same name to pass the per-request globals
 * as a parameter to the GINIT and GSHUTDOWN functions.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

/*
 * Initialize per-request globals.
 */
#ifdef TAGS
void zm_globals_ctor_newrelic(
    zend_newrelic_globals* newrelic_globals); /* ctags landing pad only */
#endif
PHP_GINIT_FUNCTION(newrelic) {
  NR_UNUSED_TSRMLS;

  php_newrelic_init_globals(newrelic_globals);
}

/*
 * Clean-up per-request globals.
 */
#ifdef TAGS
void zm_globals_dtor_newrelic(
    zend_newrelic_globals* newrelic_globals); /* ctags landing pad only */
#endif
PHP_GSHUTDOWN_FUNCTION(newrelic) {
  NR_UNUSED_TSRMLS;

  /*
   * Note that this is allocated the first time RINIT is called, rather than
   * in the more obvious GINIT function. nr_php_extension_instrument_dtor can
   * cope with an uninitialised extensions structure.
   */
  nr_php_extension_instrument_destroy(&newrelic_globals->extensions);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/*
 * Consults configuration settings and file system markers to decide if the
 * agent should start the dameon
 */
nr_daemon_startup_mode_t nr_php_get_daemon_startup_mode(void) {
  /*
   * Never launch a daemon if there exists a manual configuration file.
   * If the file /etc/newrelic/newrelic.cfg exists, the agent will never
   * start the daemon as it assumes the daemon is meant to be started by
   * its startup script, not via the agent. This setting has no meaning
   * to, and does not appear in newrelic.cfg.
   */
  if (0 == nr_access("/etc/newrelic/newrelic.cfg", F_OK)) {
    return NR_DAEMON_STARTUP_INIT;
  }

  if (3 == NR_PHP_PROCESS_GLOBALS(no_daemon_launch)) {
    /*
     * The agent will never start the daemon.
     * Use this if you are configuring the daemon via newrelic.cfg and
     * starting it outside of the agent.
     */
    return NR_DAEMON_STARTUP_INIT;
  }

  if (NR_PHP_PROCESS_GLOBALS(cli)) {
    /*
     * If command line version of PHP was used, the agent will not start the
     * daemon.
     */
    if (1 == NR_PHP_PROCESS_GLOBALS(no_daemon_launch)) {
      return NR_DAEMON_STARTUP_INIT;
    }
  } else {
    /*
     * If non-command line version of PHP was used (for example Apache or
     * php-fpm) then the agent will not start the daemon (only the command
     * line usage will start the daemon).
     */
    if (2 == NR_PHP_PROCESS_GLOBALS(no_daemon_launch)) {
      return NR_DAEMON_STARTUP_INIT;
    }
  }

  if (NULL == NR_PHP_PROCESS_GLOBALS(daemon_conn_params)) {
    nrl_verbosedebug(
        NRL_DAEMON,
        "Daemon connection information is unknown. Unable to check whether "
        "connection settings specify a host different from the local host. "
        "Daemon will not be started by the agent.");
    return NR_DAEMON_STARTUP_INIT;
  }

  if (NR_AGENT_CONN_TCP_HOST_PORT
      == NR_PHP_PROCESS_GLOBALS(daemon_conn_params)->type) {
    /*
     * Never start the daemon if the daemon connection settings specify a host
     * different from the local host
     */
    nrl_info(NRL_DAEMON,
             "Daemon connection settings specify a host different from the "
             "local host. Daemon will not be started by the Agent.");
    return NR_DAEMON_STARTUP_INIT;
  }

  return NR_DAEMON_STARTUP_AGENT;
}

/*
 * Returns : NR_FAILURE if it is a threaded MPM, and NR_SUCCESS otherwise.
 */
static nr_status_t nr_php_check_for_threaded_mpm(TSRMLS_D) {
  if ((0 != NR_PHP_PROCESS_GLOBALS(is_apache))
      && (0 != NR_PHP_PROCESS_GLOBALS(apache_threaded))) {
    NR_PHP_PROCESS_GLOBALS(mpm_bad) = 1;
    php_error_docref(
        NULL TSRMLS_CC, E_WARNING,
        "You attempted to load the New Relic module, but you appear to be "
        "using a "
        "threaded Apache MPM (--with-mpm=worker/event). This MPM is not "
        "supported by PHP or New Relic, as it has known stability issues.");
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

#define NR_PHP_UPGRADE_LICENSE_KEY_FILE "/etc/newrelic/upgrade_please.key"

static char* nr_php_check_for_upgrade_license_key(void) {
  FILE* keyfile = fopen(NR_PHP_UPGRADE_LICENSE_KEY_FILE, "r");

  if (keyfile) {
    char tmpstr[NR_LICENSE_SIZE + 16];
    size_t br = fread(tmpstr, 1, NR_LICENSE_SIZE, keyfile);

    fclose(keyfile);
    tmpstr[NR_LICENSE_SIZE] = 0;
    if (NR_LICENSE_SIZE == br) {
      return nr_strdup(tmpstr);
    }
  }

  return 0;
}

static nr_status_t nr_php_check_8T_DT_config(TSRMLS_D) {
  /* check if infinite tracing is enabled and DT disabled */
  if (!nr_strempty(NRINI(trace_observer_host))
      && !NRINI(distributed_tracing_enabled)) {
    nrl_warning(
        NRL_INIT,
        "Infinite tracing will be DISABLED because distributed tracing is"
        " disabled and infinite tracing requires distributed tracing to be "
        "enabled.  Please check the"
        " value of 'newrelic.distributed_tracing_enabled' in the agent "
        "configuration.");
    return NR_FAILURE;
  }
  return NR_SUCCESS;
}

static void nr_php_check_CAT_DT_config(TSRMLS_D) {
  if (NRINI(distributed_tracing_enabled) && NRINI(cross_process_enabled)) {
    // send a warning message to agent log
    nrl_warning(NRL_INIT,
                "Cross Application Tracing will be DISABLED because "
                "Distributed Tracing is enabled. CAT functionality has been "
                "superseded by DT and will be removed in a future release. The "
                "New Relic PHP Agent Team suggests manually disabling CAT via "
                "the 'newrelic.cross_application_tracer.enabled' INI setting "
                "in your INI file and enabling DT via the "
                "'newrelic.distributed_tracing_enabled' INI setting.");

    // set CAT INI value to disabled (just to be safe)
    NRINI(cross_process_enabled) = 0;
  }
}

/*
 * @brief Check the INI values for 'logging_enabled',
 *        'log_forwarding_enabled', and 'log_decorating_enabled' and log a
 *        warning on invalid configuration state.
 *
 */
static void nr_php_check_logging_config(TSRMLS_D) {
  if (!NRINI(logging_enabled) && NRINI(log_forwarding_enabled)) {
    nrl_warning(NRL_INIT,
                "Log Forwarding will be DISABLED because logging is disabled. "
                "Log Forwarding requires Logging to be enabled. Please check "
                "'newrelic.application_logging.logging.enabled' in the agent "
                "configuration.");
  }

  if (!NRINI(logging_enabled) && NRINI(log_decorating_enabled)) {
    nrl_warning(NRL_INIT,
                "Log Decorating will be DISABLED because logging is disabled. "
                "Log Decorating requires Logging to be enabled. Please check "
                "'newrelic.application_logging.logging.enabled' in the agent "
                "configuration.");
  }

  if (NRINI(logging_enabled) && NRINI(log_forwarding_enabled)
      && NRINI(log_decorating_enabled)) {
    nrl_warning(NRL_INIT,
                "Log Forwarding and Log Decorating have been enabled! "
                "This could lead to duplicated ingest of log messages! "
                "Check newrelic.application_logging.forwarding.enabled and "
                "newrelic.application_logging.local_decorating.enabled in the "
                "agent configuration.");
  }
}

/*
 * @brief Check the INI values for 'log_forwarding_enabled'
 *        and 'high_security' and log a warning on invalid
 *        configuration state.
 *
 */
static void nr_php_check_high_security_log_forwarding(TSRMLS_D) {
  if (NR_PHP_PROCESS_GLOBALS(high_security) && NRINI(log_forwarding_enabled)) {
    nrl_warning(
        NRL_INIT,
        "Log Forwarding will be DISABLED because High Security mode "
        "is enabled. Please check 'newrelic.high_security' in the agent "
        "configuration.");
  }
}

static char* nr_php_get_agent_specific_info(void) {
  const char* php_version;
  const char* zend_type;
  char web_server_info[512];
  char buf[512];

  web_server_info[0] = '\0';
  if (NR_PHP_PROCESS_GLOBALS(is_apache)) {
    snprintf(web_server_info, sizeof(web_server_info),
             "apache='%d.%d.%d%s' mpm=%s", NR_PHP_PROCESS_GLOBALS(apache_major),
             NR_PHP_PROCESS_GLOBALS(apache_minor),
             NR_PHP_PROCESS_GLOBALS(apache_patch),
             NR_PHP_PROCESS_GLOBALS(apache_add),
             (1 == NR_PHP_PROCESS_GLOBALS(apache_threaded)) ? "threaded"
                                                            : "prefork");
  }

  if (NR_PHP_PROCESS_GLOBALS(php_version)
      && (0 != NR_PHP_PROCESS_GLOBALS(php_version)[0])) {
    php_version = NR_PHP_PROCESS_GLOBALS(php_version);
  } else {
    php_version = "unknown";
  }

#ifdef ZTS
  zend_type = "yes";
#else
  zend_type = "no";
#endif

  buf[0] = '\0';
  snprintf(buf, sizeof(buf), " php='%s' zts=%s sapi='%s' %s", php_version,
           zend_type, sapi_module.name, web_server_info);

  return nr_strdup(buf);
}

/*
 * IMPORTANT: lifted directly out of Apache's httpd.h
 */
typedef struct nr_ap_version {
  int major;              /**< major number */
  int minor;              /**< minor number */
  int patch;              /**< patch number */
  const char* add_string; /**< additional string like "-dev" */
} nr_ap_version_t;

static void nr_php_populate_apache_process_globals(void) {
  void* handle;
  void (*mpmptr)(int, int*);
  void (*verptr)(nr_ap_version_t*);
  nr_ap_version_t av;
  int is_threaded = 0;

  handle = dlopen(0, RTLD_LAZY | RTLD_GLOBAL);

  if (0 == handle) {
    return;
  }

  mpmptr = (void (*)(int, int*))dlsym(handle, "ap_mpm_query");
  if (0 == mpmptr) {
    mpmptr = (void (*)(int, int*))dlsym(handle, "_ap_mpm_query");
  }

  verptr = (void (*)(nr_ap_version_t*))dlsym(handle, "ap_get_server_revision");
  if (0 == verptr) {
    verptr
        = (void (*)(nr_ap_version_t*))dlsym(handle, "_ap_get_server_revision");
  }

  if ((0 == mpmptr) || (0 == verptr)) {
    return;
  }

  is_threaded = 0;
  mpmptr(2 /*AP_MPMQ_IS_THREADED*/, &is_threaded);

  nr_memset(&av, 0, sizeof(av));
  verptr(&av);

  dlclose(handle);

  if (av.major) {
    NR_PHP_PROCESS_GLOBALS(is_apache) = 1;
    NR_PHP_PROCESS_GLOBALS(apache_major) = av.major;
    NR_PHP_PROCESS_GLOBALS(apache_minor) = av.minor;
    NR_PHP_PROCESS_GLOBALS(apache_patch) = av.patch;
    NR_PHP_PROCESS_GLOBALS(apache_add) = nr_strdup(av.add_string);
    NR_PHP_PROCESS_GLOBALS(apache_threaded) = is_threaded ? 1 : 0;
  }
}

static char* nr_php_get_php_version_number(TSRMLS_D) {
  char* version = 0;
  zval* php_ver = nr_php_get_constant("PHP_VERSION" TSRMLS_CC);

  if (0 == php_ver) {
    return 0;
  }

  if (nr_php_is_zval_non_empty_string(php_ver)) {
    version = nr_strndup(Z_STRVAL_P(php_ver), Z_STRLEN_P(php_ver));
  }

  nr_php_zval_free(&php_ver);

  return version;
}

#ifdef TAGS
void zm_startup_newrelic(void); /* ctags landing pad only */
#endif
PHP_MINIT_FUNCTION(newrelic) {
  nr_status_t ret;
  char* daemon_address;
  nr_daemon_startup_mode_t daemon_startup_mode;
  int daemon_connect_succeeded;
  nr_conn_params_t* conn_params;

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO /* < PHP8 */
  zend_extension dummy;
#else
  char dummy[] = "newrelic";
#endif

  (void)type;

  nr_php_global_init();
  NR_PHP_PROCESS_GLOBALS(enabled) = 1;
  NR_PHP_PROCESS_GLOBALS(our_module_number) = module_number;
  NR_PHP_PROCESS_GLOBALS(php_version) = nr_php_get_php_version_number(TSRMLS_C);
  NR_PHP_PROCESS_GLOBALS(upgrade_license_key)
      = nr_php_check_for_upgrade_license_key();
  NR_PHP_PROCESS_GLOBALS(high_security) = 0;
  NR_PHP_PROCESS_GLOBALS(preload_framework_library_detection) = 1;
  nr_php_populate_apache_process_globals();
  nr_php_api_distributed_trace_register_userland_class(TSRMLS_C);
  /*
   * The CLI SAPI reports its name as cli. The CLI Web server reports its name
   * as cli-server.
   */
  if (0 == nr_strcmp(sapi_module.name, "cli")) {
    NR_PHP_PROCESS_GLOBALS(cli) = 1;
  }

  /*
   * As of 01Jan2014, we don't even try to support Apache threaded mpm.
   * If we detect that we're running in that environment, just disable the
   * agent. There are no overrides.
   */
  if (NR_SUCCESS != nr_php_check_for_threaded_mpm(TSRMLS_C)) {
    /*
     * Here we return SUCCESS, despite the lack of it.  The global 'enabled'
     * flag prevents future execution by this module.
     *
     * See zend_startup_module_ex and zend_startup_modules within zend_API.c
     * This is tricky code:
     * Note that the return values of zend_startup_module_ex do not match those
     * expected by zend_hash_apply.
     */
    NR_PHP_PROCESS_GLOBALS(enabled) = 0;
    return SUCCESS;
  }

  /*
   * The internal function wrap records are created prior to reading the ini
   * entries so that they can be properly disabled by:
   *   newrelic.special.disable_instrumentation
   */
  nr_php_generate_internal_wrap_records();

  nr_php_register_ini_entries(module_number TSRMLS_CC);

  if (0 == NR_PHP_PROCESS_GLOBALS(enabled)) {
  disbad:
    nrl_info(NRL_INIT, "New Relic PHP Agent globally disabled");
    NR_PHP_PROCESS_GLOBALS(enabled) = 0;
    nrl_close_log_file();
    return SUCCESS;
  }

  /* Determine i) the daemon location and ii) the type of connection
   * required between the daemon and agent. Then setup the necessary
   * communication parameters required for that to happen */
  daemon_address = php_newrelic_init_daemon_path();

  nrl_info(NRL_INIT, "attempt daemon connection via '%s'", daemon_address);

  conn_params = nr_conn_params_init(daemon_address);
  NR_PHP_PROCESS_GLOBALS(daemon_conn_params) = conn_params;

  ret = nr_agent_initialize_daemon_connection_parameters(conn_params);
  if (NR_FAILURE == ret) {
    nrl_warning(NRL_INIT, "daemon connection initialization failed");
    goto disbad;
  }

  daemon_startup_mode = nr_php_get_daemon_startup_mode();

  {
    char* agent_specific_info = nr_php_get_agent_specific_info();
    nr_banner(daemon_address, daemon_startup_mode, agent_specific_info);
    nr_free(agent_specific_info);
  }

  if (0 == nr_php_use_license(0 TSRMLS_CC)) {
    nrl_warning(
        NRL_INIT,
        "A global default license has not been set or has invalid format. "
        "Please add a 'newrelic.license' key in the global php.ini or "
        "in the newrelic.ini file, or ensure that a valid license is "
        "provided on a per-virtual host or per-directory basis.");
  }

  /*
   * Attempt to connect to the daemon here.  Note that we do this no matter
   * the startup mode.  This delay allows CLI processes enough time to
   * connect. Since they handle a single request, they cannot wait through a
   * request for the connection to finish.
   */
#define NR_PHP_MINIT_DAEMON_CONNECTION_TIMEOUT_MS 10
  daemon_connect_succeeded
      = nr_agent_try_daemon_connect(NR_PHP_MINIT_DAEMON_CONNECTION_TIMEOUT_MS);

  if (0 == daemon_connect_succeeded) {
    if (NR_DAEMON_STARTUP_AGENT == daemon_startup_mode) {
      nr_daemon_args_t daemon_args;
      pid_t daemon_pid = -1;

      nr_memset(&daemon_args, 0, sizeof(daemon_args));

      daemon_args.proxy = NR_PHP_PROCESS_GLOBALS(proxy);
      daemon_args.daemon_address = daemon_address;
      daemon_args.tls_cafile = NR_PHP_PROCESS_GLOBALS(ssl_cafile);
      daemon_args.tls_capath = NR_PHP_PROCESS_GLOBALS(ssl_capath);

#define NR_PHP_DAEMON_PIDFILE "newrelic-daemon.pid"
      daemon_args.pidfile = NR_PHP_PROCESS_GLOBALS(pidfile);
      if (0 == nr_php_ini_setting_is_set_by_user("newrelic.daemon.pidfile")) {
        if (0 == nr_access("/var/run", W_OK)) {
          daemon_args.pidfile = "/var/run/" NR_PHP_DAEMON_PIDFILE;
        } else if (0 == nr_access("/var/pid", W_OK)) {
          daemon_args.pidfile = "/var/pid/" NR_PHP_DAEMON_PIDFILE;
        } else if (0 == nr_access("/var/log/newrelic", W_OK)) {
          daemon_args.pidfile = "/var/log/newrelic/" NR_PHP_DAEMON_PIDFILE;
        } else if (0 == nr_access("/var/log", W_OK)) {
          daemon_args.pidfile = "/var/log/" NR_PHP_DAEMON_PIDFILE;
        } else {
          nrl_warning(NRL_INIT,
                      "unable to find suitable pidfile location, using none");
          daemon_args.pidfile = NULL;
        }
      }

      /*
       * If `start_timeout` is set, this will be passed on to the daemon
       * via the `--wait-for-port` flag. Consequently the daemon progenitor
       * process will wait until the worker process has initialized the
       * socket (or return after the specified timeout).
       *
       * Here `start_timeout` is set to the value of the configuration setting
       * `newrelic.daemon.start_timeout`. If no timeout was set, a default of
       * `0s` is used, which causes the progenitor process to return
       * immediately, without waiting. This corresponds to legacy agent/daemon
       * behavior.
       */
      daemon_args.start_timeout = NR_PHP_PROCESS_GLOBALS(daemon_start_timeout);
      if (NULL == daemon_args.start_timeout
          || ('\0' == daemon_args.start_timeout[0])) {
        daemon_args.start_timeout = "0s";
      }

      daemon_args.logfile = NR_PHP_PROCESS_GLOBALS(daemon_logfile);
      daemon_args.loglevel = NR_PHP_PROCESS_GLOBALS(daemon_loglevel);
      daemon_args.auditlog = NR_PHP_PROCESS_GLOBALS(daemon_auditlog);
      daemon_args.app_timeout = NR_PHP_PROCESS_GLOBALS(daemon_app_timeout);
      daemon_args.integration_mode
          = NR_PHP_PROCESS_GLOBALS(daemon_special_integration);
      daemon_args.debug_http
          = NR_PHP_PROCESS_GLOBALS(daemon_special_curl_verbose);
      daemon_args.utilization = NR_PHP_PROCESS_GLOBALS(utilization);

      daemon_pid
          = nr_spawn_daemon(NR_PHP_PROCESS_GLOBALS(daemon), &daemon_args);

      if (-1 == daemon_pid) {
        goto disbad;
      }
    } else {
      nrl_warning(NRL_DAEMON,
                  "failed to connect to the newrelic-daemon.  The agent "
                  "expects a daemon "
                  "to be started externally. "
                  "Please refer to: " NR_PHP_AGENT_EXT_DOCS_URL
                  "advanced-installation/starting-php-daemon-advanced/"
                  "#daemon-external");
    }
  }

  /*
   * If this is a web server master process (eg Apache mod_php), it may
   * fork worker processes.  In order to prevent sharing of the daemon
   * connection fd, we want to close the connection before the fork.
   * If the process is not going to fork (eg CLI), then closing the connection
   * would necessitate another connect (which is quite costly using TCP).
   *
   * Previously, an atfork handler was registered in order to close
   * the connection only if a fork occurred.  However, this was problematic
   * on FreeBSD and OS X for Apache graceful restarts, presumably because the
   * atfork handler function pointer referenced extension code which could be
   * removed by Apache.
   */
  if (0 == NR_PHP_PROCESS_GLOBALS(cli)) {
    nr_agent_close_daemon_connection();
  }

  /* Do some checking of configuration settings and handle accordingly */

  /* If infinite tracing (8T) is enabled but distributed tracing (DT) is
   * disabled this is an unworkable combination because span IDs cannot be
   * assigned to segments and this causes problems in
   * axiom/nr_segment.c::nr_segment_to_span_event() Output a warning about this
   * config issue and also that 8T will be disabled
   */
  nr_php_check_8T_DT_config(TSRMLS_C);

  nr_php_check_CAT_DT_config(TSRMLS_C);

  nr_php_check_logging_config(TSRMLS_C);
  nr_php_check_high_security_log_forwarding(TSRMLS_C);

  /*
   * Save the original PHP hooks and then apply our own hooks. The agent is
   * almost fully operational now. The last remaining initialization that
   * takes place (see the function below) is called on the very first call
   * to RINIT. The reason this is done is that we want to do some work once
   * ALL extensions have been loaded. Here during the MINIT phase there may
   * still be many other extensions to come and some, like XDEBUG, are not
   * very well behaved citizens and we need to ensure certain initialization
   * tasks are run only once the PHP VM engine is ticking over fully.
   */

  NR_PHP_PROCESS_GLOBALS(orig_execute) = NR_ZEND_EXECUTE_HOOK;
  NR_ZEND_EXECUTE_HOOK = nr_php_execute;

  if (NR_PHP_PROCESS_GLOBALS(instrument_internal)) {
    nrl_info(
        NRL_AGENT,
        "enabling internal function instrumentation (this might be slow!)");

    /*
     * We use execute_internal as a fallback as that's what PHP does
     * internally: it's entirely normal for zend_execute_internal to be NULL,
     * in which case it's implied that execute_internal will be the internal
     * executor used.
     */
    NR_PHP_PROCESS_GLOBALS(orig_execute_internal)
        = zend_execute_internal ? zend_execute_internal : execute_internal;
    zend_execute_internal = nr_php_execute_internal;
  }

  /*
   * Save the SAPI module header handler so we can use our own wrapper.
   */
  NR_PHP_PROCESS_GLOBALS(orig_header_handler) = sapi_module.header_handler;
  sapi_module.header_handler = nr_php_header_handler;

#define NR_INFO_SPECIAL_FLAGS(field)                  \
  if (NR_PHP_PROCESS_GLOBALS(special_flags).field) {  \
    nrl_info(NRL_INIT, "special_flags." #field "=1"); \
  }
  NR_INFO_SPECIAL_FLAGS(no_sql_parsing);
  NR_INFO_SPECIAL_FLAGS(show_sql_parsing);
  NR_INFO_SPECIAL_FLAGS(enable_path_translated);
  NR_INFO_SPECIAL_FLAGS(no_background_jobs);
  NR_INFO_SPECIAL_FLAGS(show_executes);
  NR_INFO_SPECIAL_FLAGS(show_execute_params);
  NR_INFO_SPECIAL_FLAGS(show_execute_stack);
  NR_INFO_SPECIAL_FLAGS(show_execute_returns);
  NR_INFO_SPECIAL_FLAGS(show_executes_untrimmed);
  NR_INFO_SPECIAL_FLAGS(no_signal_handler);
  NR_INFO_SPECIAL_FLAGS(debug_autorum);
  NR_INFO_SPECIAL_FLAGS(show_loaded_files);
  NR_INFO_SPECIAL_FLAGS(debug_cat);
#undef NR_INFO_SPECIAL_FLAGS

  nr_guzzle4_minit(TSRMLS_C);
  nr_guzzle6_minit(TSRMLS_C);
  nr_laravel_minit(TSRMLS_C);
  nr_wordpress_minit();
  nr_php_set_opcode_handlers();

  nrl_debug(NRL_INIT, "MINIT processing done");
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 7.4+ */
  NR_PHP_PROCESS_GLOBALS(zend_offset) = zend_get_resource_handle(dummy);
#else
  NR_PHP_PROCESS_GLOBALS(zend_offset) = zend_get_resource_handle(&dummy);
#endif
  return (SUCCESS);
}

static void nr_php_fatal_signal_handler(int sig) {
  int fd;

  TSRMLS_FETCH();

  fd = nrl_get_log_fd();
  if (fd >= 0) {
    nr_signal_tracer_common(sig);  // TODO: nr_backtrace_fd (fd);
    nr_write(fd, NR_PSTR("PHP execution trace follows...\n"));
    nr_php_backtrace_fd(fd, -1 /* unlimited */ TSRMLS_CC);
  }

  /*
   * Reraise the signal with the default signal handler so that the OS can
   * dump core or perform any other configured action.
   */
  nr_signal_reraise(sig);
}

void nr_php_late_initialization(void) {
  TSRMLS_FETCH();
  nrl_debug(NRL_INIT, "late_init called from pid=%d", nr_getpid());

  /*
   * The applist should be created here (after the web server forks),
   * so that the applist mutexes do not need to be re-initialized.
   */
  nr_agent_applist = nr_applist_create();
  if (NULL == nr_agent_applist) {
    nrl_error(NRL_INIT, "unable to initialize applist structure");
  }

  /*
   * We have learned that the popular Xdebug extension does not "play well
   * with others" with regards to its replacement of the error handler.
   * Since they actually do want to replace it, not simply trap it, this is
   * reasonable behavior. However, it makes it difficult for us to trap and
   * forward the errors, so if a user has Xdebug loaded, we do not install
   * our own error callback handler. Otherwise, we do.
   */
  if (0 == zend_get_extension("Xdebug")) {
    NR_PHP_PROCESS_GLOBALS(orig_error_cb) = zend_error_cb;
    zend_error_cb = nr_php_error_cb;
  } else {
    nrl_warning(NRL_INIT,
                "the Xdebug extension prevents the New Relic agent from "
                "gathering errors. No errors will be recorded.");
  }

  /*
   * Install our signal handler, unless the user has set a special flag
   * telling us not to.
   */
  if (0 == (NR_PHP_PROCESS_GLOBALS(special_flags).no_signal_handler)) {
    nr_signal_handler_install(nr_php_fatal_signal_handler);
  }

  NR_PHP_PROCESS_GLOBALS(appenv) = nr_php_get_environment(TSRMLS_C);

  NR_PHP_PROCESS_GLOBALS(done_instrumentation) = 1;
  nr_php_add_internal_instrumentation(TSRMLS_C);

  nr_php_initialize_samplers();
}
