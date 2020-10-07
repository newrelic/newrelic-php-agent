/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/wait.h>

#include <errno.h>
#include <unistd.h>

#include "nr_daemon_spawn.h"
#include "nr_daemon_spawn_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_syscalls.h"

#include "tlib_main.h"

#define pass_if_argv_has_flag(A, F) \
  pass_if_argv_has_flag_f(__func__, (A), (F), __FILE__, __LINE__)

static void pass_if_argv_has_flag_f(const char* msg,
                                    const nr_argv_t* argv,
                                    const char* flag,
                                    const char* file,
                                    int line) {
  size_t i;

  for (i = 0; i < argv->count; i++) {
    if (0 == nr_strcmp(argv->data[i], flag)) {
      break;
    }
  }

  tlib_pass_if_true_f(msg, i < argv->count, file, line, "i < argv->count",
                      "i=%zu count=%zu", i, argv->count);
}

#define fail_if_argv_has_flag(A, F) \
  fail_if_argv_has_flag_f(__func__, (A), (F), __FILE__, __LINE__)

static void fail_if_argv_has_flag_f(const char* msg,
                                    const nr_argv_t* argv,
                                    const char* flag,
                                    const char* file,
                                    int line) {
  size_t i;

  for (i = 0; i < argv->count; i++) {
    if (0 == nr_strcmp(argv->data[i], flag)) {
      break;
    }
  }

  tlib_fail_if_true(msg, i < argv->count, file, line, "i < argv->count",
                    "i=%zu count=%zu", i, argv->count);
}

#define pass_if_flag_has_value(A, F, V) \
  pass_if_flag_has_value_f(__func__, (A), (F), (V))

static void pass_if_flag_has_value_f(const char* msg,
                                     const nr_argv_t* argv,
                                     const char* flag,
                                     const char* value) {
  size_t i;

  for (i = 0; i < argv->count; i++) {
    if (0 == nr_strcmp(argv->data[i], flag)) {
      break;
    }
  }

  /*
   * Check whether flag was found and is followed by a value.
   * Note: the comparison is formulated to avoid underflow if count == 0.
   *
   *   invariant: 0 <= i <= count
   *
   *   count - i == 0 => flag was not found
   *   count - i == 1 => flag was found, but was the last element
   */
  tlib_pass_if_true(msg, argv->count - i > 1, "i=%zu count=%zu", i,
                    argv->count);

  if (argv->count - i > 1) {
    tlib_pass_if_str_equal(msg, value, argv->data[i + 1]);
  }
}

static char* nr_argv_get(const nr_argv_t* argv, size_t i) {
  if (i < argv->count) {
    return argv->data[i];
  }
  return NULL;
}

static void test_argv_append(void) {
  nr_argv_t argv = {NULL, 0, 0};

  nr_argv_append(&argv, "-a");
  tlib_pass_if_true(__func__, argv.capacity > 0, "capacity=%zu", argv.capacity);
  tlib_pass_if_int_equal(__func__, 1, argv.count);
  tlib_pass_if_str_equal(__func__, "-a", nr_argv_get(&argv, 0));

  nr_argv_append(&argv, "-b");
  tlib_pass_if_int_equal(__func__, 2, argv.count);
  tlib_pass_if_str_equal(__func__, "-b", nr_argv_get(&argv, 1));

  nr_argv_append(&argv, "value");
  tlib_pass_if_int_equal(__func__, 3, argv.count);
  tlib_pass_if_str_equal(__func__, "value", nr_argv_get(&argv, 2));

  nr_argv_destroy(&argv);
}

static void test_argv_resize(void) {
  nr_argv_t argv;

  /* set the capacity explicitly so we can force a resize */
  argv.capacity = argv.count = 2;
  argv.data = (char**)nr_calloc(argv.capacity, sizeof(char*));
  argv.data[0] = nr_strdup("-1");
  argv.data[1] = nr_strdup("-2");

  nr_argv_append(&argv, "-3");
  tlib_pass_if_true(__func__, argv.capacity >= 3, "capacity=%zu",
                    argv.capacity);
  tlib_pass_if_int_equal(__func__, 3, argv.count);
  tlib_pass_if_str_equal(__func__, "-1", argv.data[0]);
  tlib_pass_if_str_equal(__func__, "-2", argv.data[1]);
  tlib_pass_if_str_equal(__func__, "-3", argv.data[2]);

  nr_argv_destroy(&argv);
}

static void test_null_daemon_args(void) {
  nr_argv_t* argv;

  argv = nr_daemon_args_to_argv("newrelic-daemon", NULL);
  tlib_pass_if_int_equal(__func__, 3, argv->count);
  tlib_pass_if_str_equal(__func__, "newrelic-daemon", nr_argv_get(argv, 0));
  tlib_pass_if_str_equal(__func__, "--agent", nr_argv_get(argv, 1));
  tlib_pass_if_null(__func__, nr_argv_get(argv, 2));

  nr_argv_destroy(argv);
  nr_free(argv);
}

static void test_daemon_address(void) {
  nr_argv_t* argv;
  nr_daemon_args_t args;

  /*
   * The daemon address represents the address of the daemon, whether it is a
   * port, a Unix-domain socket path, or an atted abstract socket.
   */
  nr_memset(&args, 0, sizeof(args));
  args.daemon_address = "/foo/bar.sock";
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);
  pass_if_flag_has_value(argv, "--port", "/foo/bar.sock");
  nr_argv_destroy(argv);
  nr_free(argv);

  nr_memset(&args, 0, sizeof(args));
  args.daemon_address = "@newrelic";
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);
  pass_if_flag_has_value(argv, "--port", "@newrelic");
  nr_argv_destroy(argv);
  nr_free(argv);

  nr_memset(&args, 0, sizeof(args));
  args.daemon_address = "9000";
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);
  pass_if_flag_has_value(argv, "--port", "9000");
  nr_argv_destroy(argv);
  nr_free(argv);
}

static void test_integration_mode_enabled(void) {
  nr_argv_t* argv;
  nr_daemon_args_t args;

  nr_memset(&args, 0, sizeof(args));
  args.integration_mode = 1;
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);

  pass_if_argv_has_flag(argv, "--integration");
  fail_if_argv_has_flag(argv, "--integration=false");

  nr_argv_destroy(argv);
  nr_free(argv);
}

static void test_integration_mode_disabled(void) {
  nr_argv_t* argv;
  nr_daemon_args_t args;

  nr_memset(&args, 0, sizeof(args));
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);

  fail_if_argv_has_flag(argv, "--integration");
  fail_if_argv_has_flag(argv, "--integration=true");

  /*
   * Integration mode is an undocumented testing interface, so we don't
   * want to unintentionally reveal its existence by explicitly disabling
   * it via the args passed to the daemon. It's up to the daemon to test
   * that integration mode is off by default.
   */
  fail_if_argv_has_flag(argv, "--integration=false");

  nr_argv_destroy(argv);
  nr_free(argv);
}

static void test_app_timeout(void) {
  nr_argv_t* argv;
  nr_daemon_args_t args;

  nr_memset(&args, 0, sizeof(args));
  args.app_timeout = "10m";
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);

  pass_if_argv_has_flag(argv, "app_timeout=10m");

  nr_argv_destroy(argv);
  nr_free(argv);
}

static void test_start_timeout(void) {
  nr_argv_t* argv;
  nr_daemon_args_t args;

  nr_memset(&args, 0, sizeof(args));

  args.start_timeout = "10s";
  argv = nr_daemon_args_to_argv("newrelic-daemon", &args);

  pass_if_flag_has_value(argv, "--wait-for-port", "10s");

  nr_argv_destroy(argv);
  nr_free(argv);
}

/* Simulate fork from the perspective of the parent process. */
static pid_t stub_fork_return_42(void) {
  return 42;
}

/* Simulate fork failing. */
static pid_t stub_fork_return_error(void) {
  errno = EAGAIN;
  return -1;
}

static void test_spawn_daemon_bad_input(const char* fake_daemon_path) {
  pid_t pid;
  nr_daemon_args_t args;
  pid_t (*saved_fork_fn)(void) = nr_daemon_fork_hook;

  nr_daemon_fork_hook = stub_fork_return_42;

  nr_memset(&args, 0, sizeof(args));
  pid = nr_spawn_daemon(NULL, &args);
  tlib_pass_if_int_equal(__func__, -1, pid);

  pid = nr_spawn_daemon(fake_daemon_path, NULL);
  tlib_pass_if_int_equal(__func__, -1, pid);

  /* restore fork hook */
  nr_daemon_fork_hook = saved_fork_fn;
}

static void test_fork_error(const char* fake_daemon_path) {
  pid_t pid;
  nr_daemon_args_t args;
  pid_t (*saved_fork_fn)(void) = nr_daemon_fork_hook;

  nr_daemon_fork_hook = stub_fork_return_error;
  nr_memset(&args, 0, sizeof(args));

  pid = nr_spawn_daemon(fake_daemon_path, &args);
  tlib_pass_if_int_equal(__func__, -1, pid);

  /* restore fork hook */
  nr_daemon_fork_hook = saved_fork_fn;
}

static void test_fork_success(const char* fake_daemon_path) {
  pid_t pid;
  nr_daemon_args_t args;
  pid_t (*saved_fork_fn)(void) = nr_daemon_fork_hook;

  nr_daemon_fork_hook = stub_fork_return_42;
  nr_memset(&args, 0, sizeof(args));

  pid = nr_spawn_daemon(fake_daemon_path, &args);
  tlib_pass_if_int_equal(__func__, 42, pid);

  /* restore fork hook */
  nr_daemon_fork_hook = saved_fork_fn;
}

#if !defined(REFERENCE_DIR)
#error "REFERENCE_DIR not defined"
#endif

/*
 * Strip timestamps from the log file and normalize the name of the
 * fake daemon process.
 */
const char* cleanup_string
    = "sed "
      "-e '/spawned daemon child/d' "
      "-e 's/^[^a-fA-F]*[0-9]*) //' "
      "-e \"s,'/bin/true','/usr/bin/true',\"";

/*
 * Wraps the call to execvp during nr_daemon_exec() so we can verify the
 * file descriptors inherited by the daemon have been properly setup.
 */
static int daemon_exec_wrapper(const char* path, char* const argv[]) {
  /* stdout and stderr should be redirected to the log file */
  nr_write(1, NR_PSTR("info: stdout should be redirected to the log file\n"));
  nr_write(2, NR_PSTR("info: stderr should be redirected to the log file\n"));

  /*
   * The original file descriptor for the log file should be closed after
   * forking, but before exec-ing the daemon.
   */
  nr_write(nrl_get_log_fd(),
           NR_PSTR("info: inherited file descriptors > 2 should be closed\n"));

  return execvp(path, argv);
}

static void test_spawn_daemon(const char* fake_daemon_path) {
  pid_t daemon_pid;
  int daemon_exit_status;
  nr_status_t st;
  nr_daemon_args_t args;
  int (*saved_exec_fn)(const char* path, char* const argv[])
      = nr_daemon_execvp_hook;

  nr_unlink("./test_daemon.tmp");
  st = nrl_set_log_file("./test_daemon.tmp");
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_exists("./test_daemon.tmp");

  st = nrl_set_log_level("verbosedebug");
  tlib_pass_if_status_success(__func__, st);

  nr_memset(&args, 0, sizeof(args));
  args.pidfile = "/tmp/daemon_test.pid";
  args.logfile = "/tmp/daemon_test.log";
  args.loglevel = "debug";
  args.daemon_address = "/tmp/newrelic.sock";
  args.auditlog = "/tmp/daemon_test_audit.log";
  args.proxy = "localhost:8080";
  args.tls_cafile = "/tmp/cafile";
  args.tls_capath = "/tmp/capath";
  args.utilization = (nr_utilization_t){
      /* Set all other flags to 0. */
      .docker = 1,
  };

  nr_daemon_execvp_hook = daemon_exec_wrapper;
  daemon_pid = nr_spawn_daemon(fake_daemon_path, &args);
  nrl_close_log_file();

  tlib_fail_if_int_equal(__func__, -1, daemon_pid);

  if (-1 != daemon_pid) {
    waitpid(daemon_pid, &daemon_exit_status, 0);
    tlib_pass_if_not_diff("./test_daemon.tmp", REFERENCE_DIR "/test_daemon.cmp",
                          cleanup_string, 0, 0);
  }

  /* restore execvp hook */
  nr_daemon_execvp_hook = saved_exec_fn;
}

/*
 * This test has not been reworked to run in parallel.
 */
tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  const char* fake_daemon_path = NULL;

  test_argv_append();
  test_argv_resize();
  test_null_daemon_args();
  test_daemon_address();
  test_integration_mode_enabled();
  test_integration_mode_disabled();
  test_app_timeout();
  test_start_timeout();

  /*
   * We don't have a daemon, so we use true instead because it exits
   * immediately with success and ignores any arguments. This simulates
   * a daemon spawning and then immediately exiting as it detaches and
   * runs in it's own session.
   */
  if (0 == nr_access("/usr/bin/true", X_OK)) {
    fake_daemon_path = "/usr/bin/true";
  } else {
    fake_daemon_path = "/bin/true";
  }

  test_spawn_daemon_bad_input(fake_daemon_path);
  test_fork_error(fake_daemon_path);
  test_fork_success(fake_daemon_path);
  test_spawn_daemon(fake_daemon_path);
}
