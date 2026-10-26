/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#ifdef ZTS
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "nr_banner.h"
#include "php_agent.h"
#include "php_globals.h"
#include "php_internal_instrument.h"
#include "php_newrelic.h"
#include "php_user_instrument.h"
#include "php_user_instrument_wraprec_hashmap.h"
#include "nr_commands.h"
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
#define TORN_READ_REFRESHES 20000
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
      "newrelic.special=no_sql_parsing,show_executes\n");

  tlib_pass_if_str_equal("MINIT applies a string setting", "first.example.com",
                         NR_PHP_PROCESS_GLOBALS(collector));
  tlib_pass_if_int_equal("MINIT applies the no_sql_parsing flag", 1,
                         NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing);
  tlib_pass_if_int_equal("MINIT applies the show_executes flag", 1,
                         NR_PHP_PROCESS_GLOBALS(special_flags).show_executes);

  /* Different from what the INI says, so a rerun would change them. */
  NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).show_executes = 0;
  NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated = 1;

  zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);

  tlib_pass_if_int_equal("a refresh keeps no_sql_parsing", 0,
                         NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing);
  tlib_pass_if_int_equal("a refresh keeps show_executes", 0,
                         NR_PHP_PROCESS_GLOBALS(special_flags).show_executes);
  tlib_pass_if_int_equal(
      "a refresh keeps enable_path_translated", 1,
      NR_PHP_PROCESS_GLOBALS(special_flags).enable_path_translated);
  tlib_php_engine_destroy();
}

/*
 * A bad daemon address makes MINIT disable the agent for the whole process
 * (it takes "goto disbad": log file closed, no app list). A thread started
 * afterwards must not turn the agent back on.
 */
static void* disabled_agent_thread(void* arg NRUNUSED) {
  (void)ts_resource(0);
  end_tsrm_thread();
  return NULL;
}

static void test_new_thread_does_not_reenable_a_disabled_agent(void) {
  pthread_t thread;

  tlib_php_engine_create("newrelic.daemon.address=relative/path\n");

  tlib_pass_if_int_equal("a bad daemon address disables the agent in MINIT", 0,
                         NR_PHP_PROCESS_GLOBALS(enabled));
  tlib_pass_if_int_equal("MINIT closes the log file of a disabled agent", -1,
                         nrl_get_log_fd());

  pthread_create(&thread, NULL, disabled_agent_thread, NULL);
  pthread_join(thread, NULL);

  tlib_pass_if_int_equal("a new thread must not re-enable a disabled agent", 0,
                         NR_PHP_PROCESS_GLOBALS(enabled));
  tlib_pass_if_int_equal("a new thread must not reopen the log file", -1,
                         nrl_get_log_fd());

  tlib_php_request_start();
  tlib_pass_if_null("a request on a disabled agent starts no transaction",
                    NRPRG(txn));
  tlib_php_request_end();

  tlib_php_engine_destroy();
}

/*
 * Starts a thread and checks that it leaves a disabled agent disabled: the
 * process stays disabled, the log file stays closed and a request starts no
 * transaction.
 */
static void check_new_thread_keeps_agent_disabled(const char* when) {
  pthread_t thread;

  pthread_create(&thread, NULL, disabled_agent_thread, NULL);
  pthread_join(thread, NULL);

  tlib_pass_if_int_equal(when, 0, NR_PHP_PROCESS_GLOBALS(enabled));
  tlib_pass_if_int_equal(when, -1, nrl_get_log_fd());

  tlib_php_request_start();
  tlib_pass_if_null(when, NRPRG(txn));
  tlib_php_request_end();
}

/*
 * MINIT also disables the agent when it has to start the daemon and cannot. The
 * socket below does not exist, so the connection attempt fails, and the agent
 * would start the daemon itself. The daemon below does not exist either, so
 * nr_spawn_daemon fails before it forks and MINIT takes "goto disbad".
 */
static void test_new_thread_does_not_reenable_after_daemon_spawn_failure(void) {
  if (0 == nr_access("/etc/newrelic/newrelic.cfg", F_OK)) {
    /* With a manual configuration file the agent never starts the daemon. */
    return;
  }

  /* The engine's fake daemon connection would make the connection succeed. */
  tlib_php_use_fake_daemon_fd = 0;
  tlib_php_engine_create(
      "newrelic.daemon.address=/nonexistent/newrelic-test.sock\n"
      "newrelic.daemon.location=/nonexistent/newrelic-daemon\n"
      "newrelic.daemon.dont_launch=0\n");
  tlib_php_use_fake_daemon_fd = 1;

  tlib_pass_if_int_equal("the agent would start the daemon itself",
                         NR_DAEMON_STARTUP_AGENT,
                         nr_php_get_daemon_startup_mode());
  tlib_pass_if_int_equal("a daemon that cannot be started disables the agent",
                         0, NR_PHP_PROCESS_GLOBALS(enabled));

  check_new_thread_keeps_agent_disabled(
      "a new thread must not undo a disable after a daemon spawn failure");

  tlib_php_engine_destroy();
}

/*
 * MINIT finds out whether it runs in a threaded Apache MPM by looking up
 * ap_mpm_query and ap_get_server_revision in the main program (dlsym). The test
 * executable exports these two, which are called only when a lookup finds
 * them, and they answer "not Apache" unless a test says otherwise.
 */
typedef struct {
  int major;
  int minor;
  int patch;
  const char* add_string;
} mock_ap_version_t;

static int mock_apache_major;
static int mock_apache_threaded;

void ap_mpm_query(int query, int* result);
void ap_get_server_revision(mock_ap_version_t* version);

void ap_mpm_query(int query NRUNUSED, int* result) {
  *result = mock_apache_threaded;
}

void ap_get_server_revision(mock_ap_version_t* version) {
  version->major = mock_apache_major;
  version->minor = 4;
  version->patch = 0;
  version->add_string = "";
}

/*
 * A threaded Apache MPM is not supported, so MINIT disables the agent, and
 * does it before the INI entries are registered. Whatever the order, a thread
 * started afterwards must leave the agent disabled.
 */
static void test_new_thread_does_not_reenable_in_threaded_mpm(void) {
  mock_apache_major = 2;
  mock_apache_threaded = 1;
  tlib_php_engine_create("");
  mock_apache_major = 0;
  mock_apache_threaded = 0;

  tlib_pass_if_int_equal("MINIT found a threaded MPM", 1,
                         NR_PHP_PROCESS_GLOBALS(mpm_bad));
  tlib_pass_if_int_equal("a threaded MPM disables the agent in MINIT", 0,
                         NR_PHP_PROCESS_GLOBALS(enabled));

  check_new_thread_keeps_agent_disabled(
      "a new thread must not undo a disable in a threaded MPM");

  tlib_php_engine_destroy();
}

/*
 * Torn reads. While a thread starts, its refresh used to clear and rebuild
 * shared state that threads already running read without a lock. Here one
 * thread polls that state while another runs many refreshes. With the guards
 * the state is never written, so the reader cannot see anything odd; without
 * them it does, with very high probability.
 */
typedef struct {
  volatile int stop;
  volatile int started;
  volatile long closed_log_fd;   /* log fd that was closed under the reader */
  volatile long empty_level;     /* error level reported as not printable */
  volatile long empty_collector; /* collector read as an empty string */
  volatile long flag_off;        /* a configured special flag read as off */
} torn_reader_t;

static void* torn_reader_thread(void* arg) {
  torn_reader_t* r = (torn_reader_t*)arg;

  r->started = 1;
  while (!r->stop) {
    int fd = nrl_get_log_fd();
    char* collector = nr_strdup(NR_PHP_PROCESS_GLOBALS(collector));

    if (fd >= 0 && -1 == fcntl(fd, F_GETFD) && EBADF == errno) {
      r->closed_log_fd++;
    }
    if (!nrl_should_print(NRL_ERROR, NRL_AGENT)) {
      r->empty_level++;
    }
    if (NULL != collector && '\0' == collector[0]) {
      r->empty_collector++;
    }
    if (0 == NR_PHP_PROCESS_GLOBALS(special_flags).no_sql_parsing) {
      r->flag_off++;
    }
    nr_free(collector);
  }
  return NULL;
}

static void test_refresh_is_invisible_to_running_threads(void) {
  static torn_reader_t reader;
  pthread_t thread;
  struct timespec start;
  struct timespec now;
  int i;

  memset(&reader, 0, sizeof(reader));
  tlib_php_engine_create(
      "newrelic.daemon.collector_host=collector.example.com\n"
      "newrelic.special=no_sql_parsing\n");

  pthread_create(&thread, NULL, torn_reader_thread, &reader);
  while (!reader.started) {
    sched_yield();
  }
  /*
   * Stop after a fixed number of refreshes or after a second, whichever comes
   * first: under valgrind a refresh is far slower.
   */
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < TORN_READ_REFRESHES; i++) {
    zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec - start.tv_sec >= 1) {
      break;
    }
  }
  reader.stop = 1;
  pthread_join(thread, NULL);

  printf(
      "TORN closed_fd=%ld empty_level=%ld empty_collector=%ld flag_off=%ld\n",
      reader.closed_log_fd, reader.empty_level, reader.empty_collector,
      reader.flag_off);
  tlib_pass_if_long_equal("the log fd is never closed under a reader", 0,
                          reader.closed_log_fd);
  tlib_pass_if_long_equal("the log level mask is never empty for a reader", 0,
                          reader.empty_level);
  tlib_pass_if_long_equal("collector is never empty for a reader", 0,
                          reader.empty_collector);
  tlib_pass_if_long_equal("a configured special flag never reads as off", 0,
                          reader.flag_off);
  tlib_php_engine_destroy();
}

/*
 * The guards, and not guarding settings that only MINIT reads, rely on PHP
 * running the refreshes of different new threads one at a time:
 * ts_resource_ex holds tsmm_mutex while it runs the end-of-thread handler
 * that refreshes the INI. This wraps that handler, counts how many run at the
 * same time (sleeping inside so an overlap would show), and requires one.
 */
static tsrm_thread_end_func_t overlap_original_handler;
static volatile int overlap_running;
static volatile int overlap_max;
static pthread_barrier_t overlap_barrier;

static void overlap_counting_handler(THREAD_T thread_id) {
  int now = __sync_add_and_fetch(&overlap_running, 1);
  int seen;

  while ((seen = overlap_max) < now
         && !__sync_bool_compare_and_swap(&overlap_max, seen, now)) {
  }
  usleep(300);
  if (NULL != overlap_original_handler) {
    overlap_original_handler(thread_id);
  }
  __sync_fetch_and_sub(&overlap_running, 1);
}

static void* overlap_thread(void* arg NRUNUSED) {
  pthread_barrier_wait(&overlap_barrier);
  (void)ts_resource(0);
  end_tsrm_thread();
  return NULL;
}

static void test_refreshes_of_new_threads_do_not_overlap(void) {
  enum { THREADS = 16, ROUNDS = 20 };
  pthread_t threads[THREADS];
  int round;
  int i;

  tlib_php_engine_create("");
  overlap_max = 0;
  overlap_original_handler
      = (tsrm_thread_end_func_t)tsrm_set_new_thread_end_handler(
          overlap_counting_handler);

  for (round = 0; round < ROUNDS; round++) {
    pthread_barrier_init(&overlap_barrier, NULL, THREADS);
    for (i = 0; i < THREADS; i++) {
      pthread_create(&threads[i], NULL, overlap_thread, NULL);
    }
    for (i = 0; i < THREADS; i++) {
      pthread_join(threads[i], NULL);
    }
    pthread_barrier_destroy(&overlap_barrier);
  }

  (void)tsrm_set_new_thread_end_handler(overlap_original_handler);
  tlib_pass_if_int_equal("refreshes of new threads run one at a time", 1,
                         overlap_max);
  tlib_php_engine_destroy();
}

/*
 * A refresh must not close and reopen a log file that is open. The marker is a
 * file status flag on the open file description; reopening the file creates a
 * new description without it. (The fd number is usually reused, so comparing
 * numbers would not show the reopen.)
 */
static void test_refresh_keeps_an_open_log_file(void) {
  int fd;
  int flags;

  tlib_php_engine_create("");

  fd = nrl_get_log_fd();
  tlib_pass_if_true("the log file is open", fd >= 0, "fd=%d", fd);
  flags = fcntl(fd, F_GETFL);
  tlib_pass_if_int_equal("the marker is set on the open log file", 0,
                         fcntl(fd, F_SETFL, flags | O_NONBLOCK));

  zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);

  fd = nrl_get_log_fd();
  tlib_pass_if_true("the log file is still open", fd >= 0, "fd=%d", fd);
  tlib_pass_if_true("a refresh must not reopen the log file",
                    fd >= 0 && (fcntl(fd, F_GETFL) & O_NONBLOCK),
                    "the marker is gone");
  tlib_php_engine_destroy();
}
/*
 * The tests above call the refresh directly. These start real threads, so the
 * whole path PHP takes when a thread starts runs, and anything on it that
 * changes the log file or the log level under running threads shows. One
 * thread is started at a time, until REAL_THREAD_STARTS or one second, because
 * under valgrind a thread start is far slower.
 */
#define REAL_THREAD_STARTS 400

static void* real_start_thread(void* arg NRUNUSED) {
  (void)ts_resource(0);
  end_tsrm_thread();
  return NULL;
}

static void start_real_threads(void) {
  struct timespec start;
  struct timespec now;
  int i;

  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < REAL_THREAD_STARTS; i++) {
    pthread_t thread;

    pthread_create(&thread, NULL, real_start_thread, NULL);
    pthread_join(thread, NULL);
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec - start.tv_sec >= 1) {
      break;
    }
  }
}

static void test_real_thread_starts_are_invisible_to_running_threads(void) {
  static torn_reader_t reader;
  pthread_t thread;

  memset(&reader, 0, sizeof(reader));
  tlib_php_engine_create(
      "newrelic.daemon.collector_host=collector.example.com\n"
      "newrelic.special=no_sql_parsing\n");

  pthread_create(&thread, NULL, torn_reader_thread, &reader);
  while (!reader.started) {
    sched_yield();
  }
  start_real_threads();
  reader.stop = 1;
  pthread_join(thread, NULL);

  printf(
      "REAL closed_fd=%ld empty_level=%ld empty_collector=%ld flag_off=%ld\n",
      reader.closed_log_fd, reader.empty_level, reader.empty_collector,
      reader.flag_off);
  tlib_pass_if_long_equal(
      "a thread start never closes the log fd under a reader", 0,
      reader.closed_log_fd);
  tlib_pass_if_long_equal(
      "a thread start never leaves the log level mask empty for a reader", 0,
      reader.empty_level);
  tlib_pass_if_long_equal("a thread start never empties collector for a reader",
                          0, reader.empty_collector);
  tlib_pass_if_long_equal(
      "a thread start never turns a configured special flag off for a reader",
      0, reader.flag_off);
  tlib_php_engine_destroy();
}

/*
 * What a lost log line looks like from the outside: a thread writes numbered
 * lines at the most verbose level while other threads start, and every one of
 * them must be in the log file afterwards. A line is lost when the level mask
 * is empty at that moment, or when the log fd is closed.
 */
#define LOG_LINE_MARKER "thread-start-marker"
#define LOG_WRITER_MAX_LINES 300000

typedef struct {
  volatile int stop;
  volatile int started;
  volatile long attempted;
} log_writer_t;

static void* log_writer_thread(void* arg) {
  log_writer_t* writer = (log_writer_t*)arg;

  writer->started = 1;
  while (!writer->stop && writer->attempted < LOG_WRITER_MAX_LINES) {
    writer->attempted++;
    nrl_verbosedebug(NRL_AGENT, LOG_LINE_MARKER " %ld", writer->attempted);
  }
  return NULL;
}

static long count_log_lines(const char* path, const char* marker) {
  char line[1024];
  long count = 0;
  FILE* file = fopen(path, "r");

  if (NULL == file) {
    return -1;
  }
  while (NULL != fgets(line, sizeof(line), file)) {
    if (NULL != strstr(line, marker)) {
      count++;
    }
  }
  fclose(file);
  return count;
}

static void test_log_lines_survive_real_thread_starts(void) {
  static log_writer_t writer;
  pthread_t thread;
  const char* path;
  long written;

  memset(&writer, 0, sizeof(writer));
  tlib_php_engine_create("");

  path = zend_ini_string("newrelic.logfile", sizeof("newrelic.logfile") - 1, 0);
  tlib_pass_if_true("the log file path is known",
                    NULL != path && '\0' != path[0], "path=%s",
                    NRBLANKSTR(path));
  if (NULL == path || '\0' == path[0]) {
    tlib_php_engine_destroy();
    return;
  }

  pthread_create(&thread, NULL, log_writer_thread, &writer);
  while (!writer.started) {
    sched_yield();
  }
  start_real_threads();
  writer.stop = 1;
  pthread_join(thread, NULL);

  written = count_log_lines(path, LOG_LINE_MARKER);
  printf("LINES attempted=%ld written=%ld\n", writer.attempted, written);
  tlib_pass_if_true("the writer logged lines while threads started",
                    writer.attempted > 0, "attempted=%ld", writer.attempted);
  tlib_pass_if_long_equal(
      "every line logged while threads start reaches the log file",
      writer.attempted, written);
  tlib_php_engine_destroy();
}
#endif /* ZTS */

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#ifdef ZTS
/*
 * A handler that only writes process-global state must do nothing on a
 * refresh. For each one, MINIT applies the INI value; the test puts a
 * sentinel in the global and refreshes. A handler that is not guarded writes
 * the INI value back over the sentinel. (Comparing a string pointer would not
 * do: malloc often hands the same address back.)
 */
#define SENTINEL_STRING "refresh-sentinel"

typedef struct {
  const char* ini_name;
  const char* ini_value;
  char** field;
} string_setting_t;

typedef struct {
  const char* ini_name;
  const char* ini_value;
  int* field;
} int_setting_t;

static void test_refresh_leaves_every_process_global_setting_alone(void) {
  size_t i;
  nrinternalfn_t* curl_exec = NULL;
  nrinternalfn_t* w;
  uint64_t original_appinfo_timeout = nr_cmd_appinfo_timeout_us;
  int original_curl_exec_disabled = 0;
  nrtime_t connect_timeout;
  nrtime_t expensive_min;
  string_setting_t strings[] = {
      {"newrelic.daemon.auditlog", "/tmp/nr-audit.log",
       &NR_PHP_PROCESS_GLOBALS(daemon_auditlog)},
      {"newrelic.daemon.logfile", "/tmp/nr-daemon.log",
       &NR_PHP_PROCESS_GLOBALS(daemon_logfile)},
      {"newrelic.daemon.loglevel", "debug",
       &NR_PHP_PROCESS_GLOBALS(daemon_loglevel)},
      {"newrelic.daemon.port", "/tmp/nr-port.sock",
       &NR_PHP_PROCESS_GLOBALS(udspath)},
      {"newrelic.daemon.address", "/tmp/nr-address.sock",
       &NR_PHP_PROCESS_GLOBALS(address_path)},
      {"newrelic.daemon.ssl_ca_bundle", "/tmp/nr-ca.pem",
       &NR_PHP_PROCESS_GLOBALS(ssl_cafile)},
      {"newrelic.daemon.ssl_ca_path", "/tmp/nr-ca-dir",
       &NR_PHP_PROCESS_GLOBALS(ssl_capath)},
      {"newrelic.daemon.proxy", "proxy.example.com:3128",
       &NR_PHP_PROCESS_GLOBALS(proxy)},
      {"newrelic.daemon.location", "/tmp/nr-daemon",
       &NR_PHP_PROCESS_GLOBALS(daemon)},
      {"newrelic.daemon.pidfile", "/tmp/nr-daemon.pid",
       &NR_PHP_PROCESS_GLOBALS(pidfile)},
      {"newrelic.daemon.app_timeout", "10m",
       &NR_PHP_PROCESS_GLOBALS(daemon_app_timeout)},
      {"newrelic.daemon.start_timeout", "5s",
       &NR_PHP_PROCESS_GLOBALS(daemon_start_timeout)},
  };
  /* Settings whose INI value may be the default are given a value that
   * differs from it, so a rewrite is visible. */
  int_setting_t ints[] = {
      {"newrelic.high_security", "0", &NR_PHP_PROCESS_GLOBALS(high_security)},
      {"newrelic.preload_framework_library_detection", "1",
       &NR_PHP_PROCESS_GLOBALS(preload_framework_library_detection)},
      {"newrelic.vulnerability_management.composer_api.per_process_detection",
       "1", &NR_PHP_PROCESS_GLOBALS(composer_api_per_process_detection)},
      {"newrelic.daemon.dont_launch", "3",
       &NR_PHP_PROCESS_GLOBALS(no_daemon_launch)},
      {"newrelic.daemon.special.curl_verbose", "1",
       &NR_PHP_PROCESS_GLOBALS(daemon_special_curl_verbose)},
      {"newrelic.daemon.special.integration", "1",
       &NR_PHP_PROCESS_GLOBALS(daemon_special_integration)},
      {"newrelic.special.enable_extension_instrumentation", "1",
       &NR_PHP_PROCESS_GLOBALS(instrument_extensions)},
      {"newrelic.transaction_tracer.internal_functions_enabled", "1",
       &NR_PHP_PROCESS_GLOBALS(instrument_internal)},
  };

  tlib_php_engine_create(
      "newrelic.daemon.auditlog=/tmp/nr-audit.log\n"
      "newrelic.daemon.logfile=/tmp/nr-daemon.log\n"
      "newrelic.daemon.loglevel=debug\n"
      "newrelic.daemon.port=/tmp/nr-port.sock\n"
      "newrelic.daemon.address=/tmp/nr-address.sock\n"
      "newrelic.daemon.ssl_ca_bundle=/tmp/nr-ca.pem\n"
      "newrelic.daemon.ssl_ca_path=/tmp/nr-ca-dir\n"
      "newrelic.daemon.proxy=proxy.example.com:3128\n"
      "newrelic.daemon.location=/tmp/nr-daemon\n"
      "newrelic.daemon.pidfile=/tmp/nr-daemon.pid\n"
      "newrelic.daemon.app_timeout=10m\n"
      "newrelic.daemon.start_timeout=5s\n"
      "newrelic.daemon.app_connect_timeout=7s\n"
      "newrelic.daemon.dont_launch=3\n"
      "newrelic.daemon.special.curl_verbose=1\n"
      "newrelic.daemon.special.integration=1\n"
      "newrelic.special.enable_extension_instrumentation=1\n"
      "newrelic.special.expensive_node_min=9ms\n"
      "newrelic.special.appinfo_timeout=3s\n"
      "newrelic.special.disable_instrumentation=curl_exec\n"
      "newrelic.transaction_tracer.internal_functions_enabled=1\n");

  /* MINIT applied each INI value, so a rewrite on refresh is detectable. */
  for (i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
    tlib_pass_if_str_equal(strings[i].ini_name, strings[i].ini_value,
                           *strings[i].field);
  }
  for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
    tlib_pass_if_int_equal(ints[i].ini_name, atoi(ints[i].ini_value),
                           *ints[i].field);
  }
  tlib_pass_if_uint64_t_equal("newrelic.daemon.app_connect_timeout",
                              7 * NR_TIME_DIVISOR,
                              NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout));
  tlib_pass_if_uint64_t_equal("newrelic.special.expensive_node_min",
                              9 * NR_TIME_DIVISOR_MS,
                              NR_PHP_PROCESS_GLOBALS(expensive_min));
  tlib_pass_if_uint64_t_equal("newrelic.special.appinfo_timeout",
                              3 * NR_TIME_DIVISOR, nr_cmd_appinfo_timeout_us);
  for (w = nr_wrapped_internal_functions; NULL != w; w = w->next) {
    if (0 == nr_strcmp(w->full_name, "curl_exec")) {
      curl_exec = w;
    }
  }
  tlib_pass_if_not_null("curl_exec has a wrap record", curl_exec);
  if (NULL == curl_exec) {
    tlib_php_engine_destroy();
    return;
  }
  tlib_pass_if_int_equal("newrelic.special.disable_instrumentation", 1,
                         curl_exec->is_disabled);

  /* Put a sentinel in every global. */
  for (i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
    nr_free(*strings[i].field);
    *strings[i].field = nr_strdup(SENTINEL_STRING);
  }
  for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
    *ints[i].field += 100;
  }
  connect_timeout = NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout);
  expensive_min = NR_PHP_PROCESS_GLOBALS(expensive_min);
  NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout) = connect_timeout + 100;
  NR_PHP_PROCESS_GLOBALS(expensive_min) = expensive_min + 100;
  nr_cmd_appinfo_timeout_us += 100;
  original_curl_exec_disabled = curl_exec->is_disabled;
  curl_exec->is_disabled = 0;

  zend_ini_refresh_caches(ZEND_INI_STAGE_STARTUP);

  for (i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
    tlib_pass_if_str_equal(strings[i].ini_name, SENTINEL_STRING,
                           *strings[i].field);
  }
  for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
    tlib_pass_if_int_equal(ints[i].ini_name, atoi(ints[i].ini_value) + 100,
                           *ints[i].field);
  }
  tlib_pass_if_uint64_t_equal(
      "newrelic.daemon.app_connect_timeout", connect_timeout + 100,
      NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout));
  tlib_pass_if_uint64_t_equal("newrelic.special.expensive_node_min",
                              expensive_min + 100,
                              NR_PHP_PROCESS_GLOBALS(expensive_min));
  tlib_pass_if_uint64_t_equal("newrelic.special.appinfo_timeout",
                              3 * NR_TIME_DIVISOR + 100,
                              nr_cmd_appinfo_timeout_us);
  tlib_pass_if_int_equal("newrelic.special.disable_instrumentation", 0,
                         curl_exec->is_disabled);

  /* Put back what the shutdown expects. */
  for (i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
    nr_free(*strings[i].field);
  }
  for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
    *ints[i].field -= 100;
  }
  NR_PHP_PROCESS_GLOBALS(daemon_app_connect_timeout) = connect_timeout;
  NR_PHP_PROCESS_GLOBALS(expensive_min) = expensive_min;
  nr_cmd_appinfo_timeout_us = original_appinfo_timeout;
  curl_exec->is_disabled = original_curl_exec_disabled;
  tlib_php_engine_destroy();
}
#endif /* ZTS */

void test_main(void* p NRUNUSED) {
  test_nr_php_get_daemon_startup_mode();
#ifdef ZTS
  test_ini_startup_refresh_keeps_process_globals();
  test_new_threads_keep_process_globals();
  test_new_thread_gets_its_own_ini_values();
  test_minit_applies_settings_refresh_keeps_them();
  test_new_thread_does_not_reenable_a_disabled_agent();
  test_new_thread_does_not_reenable_after_daemon_spawn_failure();
  test_new_thread_does_not_reenable_in_threaded_mpm();
  test_refresh_is_invisible_to_running_threads();
  test_refreshes_of_new_threads_do_not_overlap();
  test_refresh_keeps_an_open_log_file();
  test_real_thread_starts_are_invisible_to_running_threads();
  test_log_lines_survive_real_thread_starts();
  test_refresh_leaves_every_process_global_setting_alone();
#endif
}
