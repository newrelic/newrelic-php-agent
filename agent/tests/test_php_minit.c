/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#ifdef ZTS
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "nr_banner.h"
#include "php_agent.h"
#include "php_globals.h"
#include "php_newrelic.h"
#include "nr_commands.h"
#include "php_user_instrument.h"
#include "php_user_instrument_wraprec_hashmap.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_syscalls.h"

static void test_nr_php_get_daemon_startup_mode(void) {
  nr_daemon_startup_mode_t daemon_startup_mode;

  /*
   * Test that no daemon starts if a manual configuration file exists. If the
   * /etc/newrelic/newrelic.cfg exists, the agent shouldn't start the daemon as
   * it assumes the daemon is meant to be started by its startup script.
   */
  if (0 == nr_access("/etc/newrelic/newrelic.cfg", F_OK)) {
    /*
     * If the file exists, check for NR_DAEMON_STARTUP_INIT and end this test
     * because none of the other conditions will be reached.
     */
    daemon_startup_mode = nr_php_get_daemon_startup_mode();
    tlib_pass_if_int_equal(
        "no daemon starts if a manual configuration file exists",
        NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

    /*
     * None of the other conditions in nr_php_get_daemon_startup_mode()
     * will be reached so no need to run the other test checks below.
     */
    return;
  }

  /*
   * Set global values to ensure NR_DAEMON_STARTUP_AGENT is returned.
   */
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 0;
  NR_PHP_PROCESS_GLOBALS(cli) = 0;

  /*
   * Test NULL case for daemon_conn_params. Daemon connection information will
   * be unknown so it won't be started by the agent.
   */
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "daemon connection info unknown. It won't be started by the agent",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  NR_PHP_PROCESS_GLOBALS(daemon_conn_params)
      = (nr_conn_params_t*)nr_zalloc(sizeof(nr_conn_params_t));
  NR_PHP_PROCESS_GLOBALS(daemon_conn_params)->type = NR_AGENT_CONN_UNKNOWN;

  /*
   * Test daemon started by agent mode.
   */
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal("daemon will be started by agent",
                         NR_DAEMON_STARTUP_AGENT, daemon_startup_mode);

  /*
   * Test that no daemon starts if the command line version of PHP was used.
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 1;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if command line version of PHP was used",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  /*
   * Test that no daemon starts if non-command line version of PHP was used (for
   * example Apache or php-fpm) then the agent will not start the daemon
   * (only the command line usage will start the daemon)
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 0;
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 2;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if non-command line version of PHP was used",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  /*
   * Test that no daemon starts if the daemon is configured via newrelic.cfg and
   * starting it outside of the agent.
   */
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 3;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if daemon is configured via newrelic.cfg and starting "
      "it outside of the agent.",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  /*
   * Test that no daemon starts if the daemon connection settings specify
   * a host different from the local host
   */
  NR_PHP_PROCESS_GLOBALS(daemon_conn_params)->type
      = NR_AGENT_CONN_TCP_HOST_PORT;
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 0;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if the daemon connection settings specify a host "
      "different from the local host",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);
  nr_conn_params_free(nr_php_per_process_globals.daemon_conn_params);
}

#ifdef ZTS
/*
 * PHP runs every INI handler again with ZEND_INI_STAGE_STARTUP on each new
 * thread (zend_new_thread_end_handler -> zend_ini_refresh_caches). That must
 * not redo the process-global part of the handlers.
 */
#define TEST_NAMING_FN "test_ini_naming_fn"
#define TEST_CUSTOM_FN "test_ini_custom_fn"

static void test_ini_refresh_setup(void) {
  nruserfn_t* wraprec;

  tlib_php_engine_create(
      "newrelic.webtransaction.name.functions=" TEST_NAMING_FN
      "\n"
      "newrelic.transaction_tracer.custom=" TEST_CUSTOM_FN "\n");

  tlib_pass_if_int_equal("MINIT marks the INI startup as done", 1,
                         NR_PHP_PROCESS_GLOBALS(ini_startup_done));

  /* MINIT's "goto disbad" disables the agent after registering the INI. */
  NR_PHP_PROCESS_GLOBALS(enabled) = 0;

  /* The same block closes the log file. */
  nrl_close_log_file();

  /* A sentinel survives only if the handler leaves the string alone. */
  nr_free(NR_PHP_PROCESS_GLOBALS(collector));
  NR_PHP_PROCESS_GLOBALS(collector) = nr_strdup("sentinel.example.com");

  /* The INI log level is not "error", so a rerun would change the mask. */
  nrl_set_log_level("error");

  /*
   * Registration staged these wraprecs. Adding a name that is already there
   * returns the staged wraprec, so clear what staging set and see whether it
   * is set again.
   */
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      TEST_NAMING_FN, sizeof(TEST_NAMING_FN) - 1);
  tlib_pass_if_int_equal("MINIT staged the naming wraprec", 1,
                         NULL != wraprec ? wraprec->is_names_wt_simple : -1);
  if (NULL != wraprec) {
    wraprec->is_names_wt_simple = 0;
  }
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      TEST_CUSTOM_FN, sizeof(TEST_CUSTOM_FN) - 1);
  tlib_pass_if_int_equal("MINIT staged the custom tracer wraprec", 1,
                         NULL != wraprec ? wraprec->is_user_added : -1);
  if (NULL != wraprec) {
    wraprec->is_user_added = 0;
  }
}

static void test_ini_refresh_check(const char* when) {
  nruserfn_t* wraprec;

  tlib_pass_if_int_equal(when, 0, NR_PHP_PROCESS_GLOBALS(enabled));
  /* MSHUTDOWN skips its cleanup for a disabled agent. */
  NR_PHP_PROCESS_GLOBALS(enabled) = 1;
  tlib_pass_if_int_equal(when, -1, nrl_get_log_fd());
  tlib_pass_if_str_equal(when, "sentinel.example.com",
                         NR_PHP_PROCESS_GLOBALS(collector));
  tlib_pass_if_false(when, nrl_should_print(NRL_INFO, NRL_AGENT),
                     "info is masked");

  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      TEST_NAMING_FN, sizeof(TEST_NAMING_FN) - 1);
  tlib_pass_if_int_equal(when, 0,
                         NULL != wraprec ? wraprec->is_names_wt_simple : -1);
  wraprec = nr_php_user_instrument_wraprec_hashmap_ini_add(
      TEST_CUSTOM_FN, sizeof(TEST_CUSTOM_FN) - 1);
  tlib_pass_if_int_equal(when, 0,
                         NULL != wraprec ? wraprec->is_user_added : -1);
}

static void test_ini_startup_refresh_keeps_process_globals(void) {
  test_ini_refresh_setup();
  zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);
  test_ini_refresh_check("an INI refresh must leave process-global state");
  tlib_php_engine_destroy();
}

static void end_tsrm_thread(void) {
#ifdef ZEND_MAX_EXECUTION_TIMERS
  /* Delete the timer PHP created for this thread. */
  zend_max_execution_timer_shutdown();
#endif
  /* Free what request shutdown would have: the thread's working directory. */
  virtual_cwd_deactivate();
  ts_free_thread();
}

static void* new_tsrm_thread(void* arg NRUNUSED) {
  /* What FrankenPHP does in each PHP thread: PHP refreshes the INI cache. */
  (void)ts_resource(0);
  end_tsrm_thread();
  return NULL;
}

static void test_new_threads_keep_process_globals(void) {
  pthread_t threads[8];
  size_t i;

  test_ini_refresh_setup();
  for (i = 0; i < sizeof(threads) / sizeof(threads[0]); i++) {
    pthread_create(&threads[i], NULL, new_tsrm_thread, NULL);
  }
  for (i = 0; i < sizeof(threads) / sizeof(threads[0]); i++) {
    pthread_join(threads[i], NULL);
  }
  test_ini_refresh_check("new threads must leave process-global state");
  tlib_php_engine_destroy();
}

/*
 * The refresh also fills the new thread's own INI values. Skipping the
 * process-global part must leave that alone, or a new thread would run with
 * the defaults.
 */
typedef struct {
  char license[64];
  char appname[64];
} thread_ini_t;

static void* ini_reader_thread(void* arg) {
  thread_ini_t* out = (thread_ini_t*)arg;

  (void)ts_resource(0);
  nr_strxcpy(out->license, NRBLANKSTR(NRINI(license)),
             nr_strlen(NRBLANKSTR(NRINI(license))));
  nr_strxcpy(out->appname, NRBLANKSTR(NRINI(appnames)),
             nr_strlen(NRBLANKSTR(NRINI(appnames))));
  end_tsrm_thread();
  return NULL;
}

static void test_new_thread_gets_its_own_ini_values(void) {
  pthread_t threads[4];
  thread_ini_t seen[4];
  size_t i;

  tlib_php_engine_create("newrelic.appname=thread test app\n");
  nr_memset(seen, 0, sizeof(seen));

  for (i = 0; i < sizeof(threads) / sizeof(threads[0]); i++) {
    pthread_create(&threads[i], NULL, ini_reader_thread, &seen[i]);
  }
  for (i = 0; i < sizeof(threads) / sizeof(threads[0]); i++) {
    pthread_join(threads[i], NULL);
    tlib_pass_if_str_equal("a new thread gets newrelic.license",
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                           seen[i].license);
    tlib_pass_if_str_equal("a new thread gets newrelic.appname",
                           "thread test app", seen[i].appname);
  }
  tlib_php_engine_destroy();
}

/*
 * MINIT's own call of each handler (the first STARTUP-stage call) must still
 * apply the settings, and a later refresh must leave them alone.
 */
static void test_minit_applies_settings_refresh_keeps_them(void) {
  tlib_php_engine_create(
      "newrelic.daemon.collector_host=first.example.com\n"
      "newrelic.daemon.dont_launch=2\n"
      "newrelic.special.expensive_node_min=750ms\n"
      "newrelic.special=no_sql_parsing,show_executes\n"
      "newrelic.transaction_tracer.internal_functions_enabled=1\n");

  tlib_pass_if_str_equal("MINIT applies a string setting", "first.example.com",
                         NR_PHP_PROCESS_GLOBALS(collector));
  tlib_pass_if_int_equal("MINIT applies dont_launch", 2,
                         NR_PHP_PROCESS_GLOBALS(no_daemon_launch));
  tlib_pass_if_time_equal("MINIT applies expensive_node_min",
                          750 * NR_TIME_DIVISOR_MS,
                          NR_PHP_PROCESS_GLOBALS(expensive_min));
  tlib_pass_if_int_equal("MINIT applies the no_sql_parsing flag", 1,
                         NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing);
  tlib_pass_if_int_equal("MINIT applies the show_executes flag", 1,
                         NR_PHP_PROCESS_GLOBALS(special_flags).show_executes);
  tlib_pass_if_int_equal("MINIT applies internal_functions_enabled", 1,
                         NR_PHP_PROCESS_GLOBALS(instrument_internal));

  /* Different from what the INI says, so a rerun would change them. */
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 0;
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 1;
  NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_executes = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated = 1;
  NR_PHP_PROCESS_GLOBALS(instrument_internal) = 0;

  zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);

  tlib_pass_if_int_equal("a refresh keeps no_daemon_launch", 0,
                         NR_PHP_PROCESS_GLOBALS(no_daemon_launch));
  tlib_pass_if_time_equal("a refresh keeps expensive_min", 1,
                          NR_PHP_PROCESS_GLOBALS(expensive_min));
  tlib_pass_if_int_equal("a refresh keeps no_sql_parsing", 0,
                         NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing);
  tlib_pass_if_int_equal("a refresh keeps show_executes", 0,
                         NR_PHP_PROCESS_GLOBALS(special_flags).show_executes);
  tlib_pass_if_int_equal(
      "a refresh keeps enable_path_translated", 1,
      NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated);
  tlib_pass_if_int_equal("a refresh keeps instrument_internal", 0,
                         NR_PHP_PROCESS_GLOBALS(instrument_internal));
  tlib_php_engine_destroy();
}

/*
 * Every process-global string and number that an INI handler writes. For each
 * one: set a value the INI cannot produce, refresh, and check it is still
 * there. A handler that repeats its write on the refresh overwrites it.
 */
#define REFRESH_STRING_FIELDS(X) \
  X(daemon_auditlog)             \
  X(daemon_logfile)              \
  X(daemon_loglevel)             \
  X(udspath)                     \
  X(address_path)                \
  X(ssl_cafile)                  \
  X(ssl_capath)                  \
  X(collector)                   \
  X(proxy)                       \
  X(daemon)                      \
  X(pidfile)                     \
  X(daemon_app_timeout)          \
  X(daemon_start_timeout)

#define REFRESH_INT_FIELDS(X)            \
  X(high_security)                       \
  X(preload_framework_library_detection) \
  X(composer_api_per_process_detection)  \
  X(no_daemon_launch)                    \
  X(daemon_special_curl_verbose)         \
  X(daemon_special_integration)          \
  X(instrument_extensions)               \
  X(instrument_internal)

#define REFRESH_TIME_FIELDS(X)  \
  X(daemon_app_connect_timeout) \
  X(expensive_min)

/* One bit each, so flip the value instead of using a sentinel. */
#define REFRESH_UTILIZATION_FIELDS(X) \
  X(aws) X(azure) X(gcp) X(pcf) X(docker) X(kubernetes)

#define REFRESH_SENTINEL 12345
#define REFRESH_SENTINEL_STR "sentinel."

static void test_refresh_keeps_every_setting(void) {
  int flipped[6];
  int n;

  /* Settings that a handler only writes when the INI value is not empty. */
  tlib_php_engine_create(
      "newrelic.daemon.dont_launch=2\n"
      "newrelic.daemon.special.curl_verbose=1\n"
      "newrelic.daemon.special.integration=1\n"
      "newrelic.daemon.app_connect_timeout=3s\n"
      "newrelic.special.appinfo_timeout=5s\n"
      "newrelic.special.expensive_node_min=750ms\n"
      "newrelic.high_security=0\n"
      "newrelic.special=no_sql_parsing\n"
      "newrelic.transaction_tracer.internal_functions_enabled=1\n");

#define SET_STRING(f)                 \
  nr_free(NR_PHP_PROCESS_GLOBALS(f)); \
  NR_PHP_PROCESS_GLOBALS(f) = nr_strdup(REFRESH_SENTINEL_STR #f);
  REFRESH_STRING_FIELDS(SET_STRING)
#undef SET_STRING
#define SET_NUMBER(f) NR_PHP_PROCESS_GLOBALS(f) = REFRESH_SENTINEL;
  REFRESH_INT_FIELDS(SET_NUMBER)
  REFRESH_TIME_FIELDS(SET_NUMBER)
#undef SET_NUMBER
  n = 0;
#define FLIP(f)                                        \
  flipped[n] = !NR_PHP_PROCESS_GLOBALS(utilization).f; \
  NR_PHP_PROCESS_GLOBALS(utilization).f = flipped[n];  \
  n++;
  REFRESH_UTILIZATION_FIELDS(FLIP)
#undef FLIP
  nr_cmd_appinfo_timeout_us = REFRESH_SENTINEL;

  zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);

#define CHECK_STRING(f)                                                  \
  tlib_pass_if_str_equal("a refresh keeps " #f, REFRESH_SENTINEL_STR #f, \
                         NR_PHP_PROCESS_GLOBALS(f));
  REFRESH_STRING_FIELDS(CHECK_STRING)
#undef CHECK_STRING
#define CHECK_NUMBER(f)                                            \
  tlib_pass_if_time_equal("a refresh keeps " #f, REFRESH_SENTINEL, \
                          NR_PHP_PROCESS_GLOBALS(f));
  REFRESH_INT_FIELDS(CHECK_NUMBER)
  REFRESH_TIME_FIELDS(CHECK_NUMBER)
#undef CHECK_NUMBER
  n = 0;
#define CHECK_FLIP(f)                                                   \
  tlib_pass_if_int_equal("a refresh keeps utilization." #f, flipped[n], \
                         NR_PHP_PROCESS_GLOBALS(utilization).f);        \
  n++;
  REFRESH_UTILIZATION_FIELDS(CHECK_FLIP)
#undef CHECK_FLIP
  tlib_pass_if_time_equal("a refresh keeps the appinfo timeout",
                          REFRESH_SENTINEL, nr_cmd_appinfo_timeout_us);

  /* Not every one of these is freed at MSHUTDOWN. */
#define FREE_STRING(f)                \
  nr_free(NR_PHP_PROCESS_GLOBALS(f)); \
  NR_PHP_PROCESS_GLOBALS(f) = NULL;
  REFRESH_STRING_FIELDS(FREE_STRING)
#undef FREE_STRING
  tlib_php_engine_destroy();
}

/*
 * Catch-all for settings the table above does not list, including ones added
 * later: poison the whole process-global struct, refresh, and require every
 * byte to be as poisoned. A handler that writes any field changes it, and one
 * that frees a string or object in it crashes on the poison pointer. This
 * runs in a child so that a crash is a failure and not the end of the tests.
 */
static void test_refresh_writes_no_process_global(void) {
  pid_t pid;
  int status = 0;

  tlib_php_engine_create("");
  fflush(NULL);

  pid = fork();
  if (0 == pid) {
    static nrphpglobals_t saved;
    static nrphpglobals_t expected;
    int same;

    memcpy(&saved, &nr_php_per_process_globals, sizeof(saved));
    memset(&nr_php_per_process_globals, 0xA5,
           sizeof(nr_php_per_process_globals));
    NR_PHP_PROCESS_GLOBALS(ini_startup_done) = 1;
    memcpy(&expected, &nr_php_per_process_globals, sizeof(expected));

    zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);

    same
        = 0 == memcmp(&expected, &nr_php_per_process_globals, sizeof(expected));
    /* Put the pointers back and shut the engine down, so a leak check of
     * this child stays quiet. */
    memcpy(&nr_php_per_process_globals, &saved, sizeof(saved));
    tlib_php_engine_destroy();
    _exit(same ? 0 : 1);
  }
  waitpid(pid, &status, 0);
  tlib_pass_if_true("a refresh writes no process-global field and frees none",
                    WIFEXITED(status) && 0 == WEXITSTATUS(status),
                    "child status 0x%x", status);

  tlib_php_engine_destroy();
}
#endif /* ZTS */

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_nr_php_get_daemon_startup_mode();
#ifdef ZTS
  test_ini_startup_refresh_keeps_process_globals();
  test_new_threads_keep_process_globals();
  test_new_thread_gets_its_own_ini_values();
  test_minit_applies_settings_refresh_keeps_them();
  test_refresh_keeps_every_setting();
  test_refresh_writes_no_process_global();
#endif
}
