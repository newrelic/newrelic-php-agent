/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests of the agent's fatal signal handler (nr_php_fatal_signal_handler).
 *
 * The handler ends by re-raising the signal, which ends the process, so each
 * case runs in a child process and the parent checks how the child died: a
 * fatal signal in the handler's own code replaces the original signal.
 */
#include "tlib_php.h"
#include "php_agent.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * These tests make a thread fault on purpose, and the forked children end on a
 * signal without the engine's shutdown. The agent's unit tests run under
 * valgrind in CI, which would report the fault and the leaks of those children;
 * tests/valgrind-suppressions has entries for them, matched by this file's own
 * functions (a client request cannot switch off valgrind's check for leaks when
 * a process ends).
 */

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#ifdef ZTS

/* A child that hangs is killed by SIGALRM instead of hanging the suite. */
#define CHILD_TIMEOUT_SECONDS 60

/*
 * Runs in the child. Starts the engine and a request, so that the agent's
 * late initialization has installed the fatal signal handler.
 */
static void child_start_engine(void) {
  alarm(CHILD_TIMEOUT_SECONDS);
  tlib_php_engine_create("" PTSRMLS_CC);
  tlib_php_request_start();
}

/* The signal the child's thread raises; set before the fork. */
static int child_signal = SIGABRT;

/*
 * The thread of a server like FrankenPHP at the end of its life: PHP frees the
 * thread's storage (ts_free_thread), then a fatal signal arrives, for example
 * the abort of the C library on a corrupted heap.
 */
static void* thread_signal_after_free(void* arg NRUNUSED) {
  (void)ts_resource(0);
  ZEND_TSRMLS_CACHE_UPDATE();
  ts_free_thread();
  raise(child_signal);
  return NULL;
}

static void* thread_signal_in_request(void* arg NRUNUSED) {
  (void)ts_resource(0);
  ZEND_TSRMLS_CACHE_UPDATE();
  raise(child_signal);
  return NULL;
}

/* A thread that PHP does not know (a thread of the server's runtime, for
 * example). */
static void* thread_signal_unknown(void* arg NRUNUSED) {
  raise(child_signal);
  return NULL;
}

static void child_signal_in_thread(void* (*fn)(void*)) {
  pthread_t thread;

  child_start_engine();
  if (0 != pthread_create(&thread, NULL, fn, NULL)) {
    _exit(98);
  }
  pthread_join(thread, NULL);
  _exit(99); /* not reached: the thread raises a fatal signal */
}

/*
 * Returns how the child ended: the signal number, or 0 if it was not ended by
 * a signal.
 */
static int run_child(const char* name, int sig, void* (*fn)(void*)) {
  pid_t pid;
  int status = 0;

  child_signal = sig;
  fflush(NULL);
  pid = fork();
  if (0 == pid) {
    child_signal_in_thread(fn);
    _exit(97);
  }
  tlib_pass_if_true(name, pid > 0, "fork failed");
  if (pid <= 0) {
    return 0;
  }
  if (pid != waitpid(pid, &status, 0)) {
    return 0;
  }
  return WIFSIGNALED(status) ? WTERMSIG(status) : 0;
}

/* The signals the handler is installed for. */
static const int fatal_signals[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT};

/*
 * The process has to end with the signal that was raised, not with another
 * one from the handler's own code.
 */
static void test_signal_in_thread(const char* what, void* (*fn)(void*)) {
  size_t i;
  char name[128];

  for (i = 0; i < sizeof(fatal_signals) / sizeof(fatal_signals[0]); i++) {
    int sig = fatal_signals[i];

    snprintf(name, sizeof(name), "signal %d, %s", sig, what);
    tlib_pass_if_int_equal(name, sig, run_child(name, sig, fn));
  }
}

/*
 * The PHP stack of a thread, asked for in this process, for each state a thread
 * can be in:
 *  - with its PHP storage: the stack is read, and there is no "not available"
 * message;
 *  - after ts_free_thread, or a thread that PHP does not know:
 * tsrm_get_ls_cache() is NULL, so nothing is read, and the message says that
 * the thread has no PHP state;
 *  - a thread whose TSRM key points at memory that cannot be read (the storage
 * was damaged, or freed in a way the key does not show): reading faults, the
 * guard catches it, and the message says that reading failed;
 *  - after ts_free_thread and a new ts_resource: the stack is read as for a
 * live thread. Each call has to come back, and the thread has to be able to
 * end.
 */
typedef enum {
  THREAD_LIVE,
  THREAD_FREED,
  THREAD_UNKNOWN,
  THREAD_UNREADABLE,
  THREAD_REALLOCATED
} thread_kind_t;

typedef struct {
  int fd;
  thread_kind_t kind;
} guard_args_t;

/*
 * In a thread that has its PHP storage: makes the current execute data point at
 * a page that cannot be read, as damaged storage would, so that the guard's
 * read of the first frame faults. Returns the page.
 */
static void* make_frame_unreadable(void) {
  void* page = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  EG(current_execute_data) = (zend_execute_data*)page;
  return page;
}

static void restore_frame(void* page) {
  EG(current_execute_data) = NULL;
  munmap(page, 4096);
}

static void prepare_thread(thread_kind_t kind) {
  switch (kind) {
    case THREAD_LIVE:
      (void)ts_resource(0);
      ZEND_TSRMLS_CACHE_UPDATE();
      break;
    case THREAD_FREED:
      (void)ts_resource(0);
      ZEND_TSRMLS_CACHE_UPDATE();
      ts_free_thread();
      break;
    case THREAD_UNKNOWN:
      break;
    case THREAD_UNREADABLE:
      (void)ts_resource(0);
      ZEND_TSRMLS_CACHE_UPDATE();
      break;
    case THREAD_REALLOCATED:
      (void)ts_resource(0);
      ts_free_thread();
      (void)ts_resource(0);
      ZEND_TSRMLS_CACHE_UPDATE();
      break;
  }
}

/*
 * A thread whose current execute data is unreadable, as damaged storage would
 * leave it, that then raises a signal: the handler's own read of the PHP stack
 * faults for real (a SIGSEGV from the kernel inside the handler). The guard has
 * to catch that, and the process has to end with the signal that was raised.
 */
static void* thread_signal_unreadable(void* arg NRUNUSED) {
  (void)ts_resource(0);
  ZEND_TSRMLS_CACHE_UPDATE();
  (void)make_frame_unreadable();
  raise(child_signal);
  return NULL;
}

/* The kind of thread that takes a hardware fault, and the fault. */
static thread_kind_t child_fault_thread = THREAD_LIVE;
static int child_fault = SIGSEGV;

/*
 * The original signal is itself a fault of the hardware: a write through a null
 * pointer (SIGSEGV), or an access beyond the end of a mapped file (SIGBUS).
 */
static void* thread_hardware_fault(void* arg NRUNUSED) {
  prepare_thread(child_fault_thread);
  if (SIGBUS == child_fault) {
    char name[] = "/tmp/nr-fatal-signal-XXXXXX";
    int file = mkstemp(name);
    char* beyond_the_end;

    if (file < 0) {
      _exit(96);
    }
    unlink(name);
    beyond_the_end
        = (char*)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (MAP_FAILED == (void*)beyond_the_end) {
      _exit(95);
    }
    *(volatile char*)beyond_the_end = 1;
  } else {
    volatile int* null_pointer = NULL;

    *null_pointer = 1;
  }
  return NULL;
}

static void* thread_guarded_backtrace(void* p) {
  guard_args_t* args = (guard_args_t*)p;
  void* page = NULL;

  prepare_thread(args->kind);
  if (THREAD_UNREADABLE == args->kind) {
    page = make_frame_unreadable();
  }
  nr_php_guarded_backtrace_fd(args->fd);
  if (THREAD_UNREADABLE == args->kind) {
    restore_frame(page);
  }
  /* a thread that was given storage frees it */
  if (THREAD_LIVE == args->kind || THREAD_REALLOCATED == args->kind
      || THREAD_UNREADABLE == args->kind) {
    ts_free_thread();
  }
  return NULL;
}

static char* guarded_backtrace_output(thread_kind_t kind) {
  char path[] = "/tmp/test_fatal_signal_handler.XXXXXX";
  guard_args_t args = {.fd = mkstemp(path), .kind = kind};
  pthread_t thread;
  char* out = NULL;
  ssize_t n;

  tlib_pass_if_true("temporary file", args.fd >= 0, "mkstemp failed");
  if (args.fd < 0) {
    return NULL;
  }
  unlink(path);
  tlib_pass_if_true(
      "thread started",
      0 == pthread_create(&thread, NULL, thread_guarded_backtrace, &args),
      "pthread_create failed");
  pthread_join(thread, NULL);
  out = (char*)calloc(1, 4096);
  lseek(args.fd, 0, SEEK_SET);
  n = read(args.fd, out, 4095);
  if (n < 0) {
    out[0] = '\0';
  }
  close(args.fd);
  return out;
}

/*
 * Several threads in the guard at once, each with a key that points at memory
 * that cannot be read, so that every one of them faults and jumps back: the
 * guard must not depend on anything shared between threads (the fault handler
 * must not be swapped in and out, for example), so every one of them comes back
 * with the message, and the handlers of the fault signals are the same as
 * before.
 */
#define GUARD_THREADS 4
#define GUARD_ROUNDS 10

static pthread_barrier_t guard_barrier;
static volatile int guard_failures;

static void* thread_guard_rounds(void* arg NRUNUSED) {
  int round;

  (void)ts_resource(0);
  ZEND_TSRMLS_CACHE_UPDATE();
  pthread_barrier_wait(&guard_barrier);
  for (round = 0; round < GUARD_ROUNDS; round++) {
    char path[] = "/tmp/test_fatal_signal_handler.XXXXXX";
    char out[256] = {0};
    int fd = mkstemp(path);
    void* page;

    if (fd < 0) {
      __sync_fetch_and_add(&guard_failures, 1);
      continue;
    }
    unlink(path);
    page = make_frame_unreadable();
    nr_php_guarded_backtrace_fd(fd);
    restore_frame(page);
    lseek(fd, 0, SEEK_SET);
    if (read(fd, out, sizeof(out) - 1) <= 0
        || NULL == strstr(out, "reading it failed")) {
      __sync_fetch_and_add(&guard_failures, 1);
    }
    close(fd);
  }
  ts_free_thread();
  return NULL;
}

static void test_guard_in_several_threads_at_once(void) {
  pthread_t threads[GUARD_THREADS];
  struct sigaction segv_before, segv_after, bus_before, bus_after;
  int i;

  sigaction(SIGSEGV, NULL, &segv_before);
  sigaction(SIGBUS, NULL, &bus_before);
  guard_failures = 0;
  pthread_barrier_init(&guard_barrier, NULL, GUARD_THREADS);
  for (i = 0; i < GUARD_THREADS; i++) {
    pthread_create(&threads[i], NULL, thread_guard_rounds, NULL);
  }
  for (i = 0; i < GUARD_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }
  pthread_barrier_destroy(&guard_barrier);
  sigaction(SIGSEGV, NULL, &segv_after);
  sigaction(SIGBUS, NULL, &bus_after);

  tlib_pass_if_int_equal("every thread came back with the message", 0,
                         guard_failures);
  tlib_pass_if_true("SIGSEGV handler unchanged",
                    segv_before.sa_handler == segv_after.sa_handler,
                    "before=%p after=%p", (void*)segv_before.sa_handler,
                    (void*)segv_after.sa_handler);
  tlib_pass_if_true("SIGBUS handler unchanged",
                    bus_before.sa_handler == bus_after.sa_handler,
                    "before=%p after=%p", (void*)bus_before.sa_handler,
                    (void*)bus_after.sa_handler);
}

static void check_guard_output(const char* what,
                               thread_kind_t kind,
                               const char* expected) {
  char* out = guarded_backtrace_output(kind);
  bool ok;

  if (NULL == expected) {
    ok = NULL != out && NULL == strstr(out, "not available");
  } else {
    ok = NULL != out && NULL != strstr(out, expected);
  }
  tlib_pass_if_true(what, ok, "output=%s", NRSAFESTR(out));
  free(out);
}

static void test_guard_in_this_process(void) {
  tlib_php_engine_create("" PTSRMLS_CC);
  tlib_php_request_start();

  check_guard_output("a thread with PHP storage: no message", THREAD_LIVE,
                     NULL);
  check_guard_output("a thread whose storage was released: no PHP state",
                     THREAD_FREED, "the thread has no PHP state");
  check_guard_output("a thread PHP does not know: no PHP state", THREAD_UNKNOWN,
                     "the thread has no PHP state");
  check_guard_output(
      "a thread whose storage cannot be read: the guard catches "
      "the fault",
      THREAD_UNREADABLE, "reading it failed");
  check_guard_output("a thread with new storage after a release: reads again",
                     THREAD_REALLOCATED, NULL);

  test_guard_in_several_threads_at_once();

  tlib_php_request_end();
  tlib_php_engine_destroy(TSRMLS_C);
}

/*
 * Hardware faults, not raise(): the handler runs for a real SIGSEGV or SIGBUS,
 * on each kind of thread, and the process has to end with that signal.
 */
static void test_hardware_fault_in_thread(void) {
  static const struct {
    const char* what;
    thread_kind_t kind;
  } kinds[] = {{"thread with PHP storage", THREAD_LIVE},
               {"thread after ts_free_thread", THREAD_FREED},
               {"thread PHP does not know", THREAD_UNKNOWN}};
  static const int faults[] = {SIGSEGV, SIGBUS};
  size_t i;
  size_t j;
  char name[128];

  for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
    for (j = 0; j < sizeof(faults) / sizeof(faults[0]); j++) {
      child_fault_thread = kinds[i].kind;
      child_fault = faults[j];
      snprintf(name, sizeof(name), "hardware fault %d, %s", faults[j],
               kinds[i].what);
      tlib_pass_if_int_equal(name, faults[j],
                             run_child(name, faults[j], thread_hardware_fault));
    }
  }
}

#endif /* ZTS */

/*
 * A SIGSEGV or SIGBUS that was sent is not a fault of the read: kill(),
 * raise(), sigqueue(), a timer and the like. Every code the kernel uses for a
 * fault is.
 */
static void check_fault(const char* what,
                        bool expected,
                        int sig,
                        const siginfo_t* info) {
  tlib_pass_if_bool_equal(what, expected, nr_php_signal_is_fault(sig, info));
}

static void test_signal_is_fault(void) {
  siginfo_t info;

  nr_memset(&info, 0, sizeof(info));

  info.si_code = SEGV_MAPERR;
  check_fault("SEGV_MAPERR", true, SIGSEGV, &info);
  info.si_code = SEGV_ACCERR;
  check_fault("SEGV_ACCERR", true, SIGSEGV, &info);
  info.si_code = BUS_ADRALN;
  check_fault("BUS_ADRALN", true, SIGBUS, &info);
  info.si_code = BUS_ADRERR;
  check_fault("BUS_ADRERR", true, SIGBUS, &info);
  info.si_code = BUS_OBJERR;
  check_fault("BUS_OBJERR", true, SIGBUS, &info);
#ifdef SI_KERNEL
  /* x86: an access to a non-canonical address. */
  info.si_code = SI_KERNEL;
  check_fault("SI_KERNEL", true, SIGSEGV, &info);
#endif

  info.si_code = SI_USER;
  check_fault("SI_USER, SIGSEGV", false, SIGSEGV, &info);
  check_fault("SI_USER, SIGBUS", false, SIGBUS, &info);
  info.si_code = SI_QUEUE;
  check_fault("SI_QUEUE", false, SIGSEGV, &info);
  info.si_code = SI_TIMER;
  check_fault("SI_TIMER", false, SIGSEGV, &info);
#ifdef SI_TKILL
  /* raise() and pthread_kill() on Linux. */
  info.si_code = SI_TKILL;
  check_fault("SI_TKILL", false, SIGSEGV, &info);
#endif

  info.si_code = SEGV_MAPERR;
  check_fault("another signal with a fault code", false, SIGABRT, &info);
  check_fault("no details is taken for a fault", true, SIGSEGV, NULL);
}

#ifndef ZTS

/*
 * Non-thread-safe PHP: the globals are plain variables, so there is no thread
 * without PHP state, but the guard is the same code, and damaged globals can
 * still make the handler's read of the PHP stack fault. The child makes the
 * current execute data point at a page that cannot be read, then takes a signal
 * on its main thread: a raised signal, or a real null write (SIGSEGV). The
 * handler's read faults for real inside the handler, the guard catches it, and
 * the process has to end with the original signal, not with a second SIGSEGV.
 */
static void child_unreadable_stack(int sig, bool hardware_fault) {
  void* page;

  alarm(60);
  tlib_php_engine_create("" PTSRMLS_CC);
  tlib_php_request_start();
  page = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  EG(current_execute_data) = (zend_execute_data*)page;
  if (hardware_fault) {
    volatile int* null_pointer = NULL;

    *null_pointer = 1;
  } else {
    raise(sig);
  }
  _exit(99); /* not reached: a fatal signal ends the process */
}

static int run_child_unreadable_stack(int sig, bool hardware_fault) {
  pid_t pid;
  int status = 0;

  fflush(NULL);
  pid = fork();
  if (0 == pid) {
    child_unreadable_stack(sig, hardware_fault);
    _exit(97);
  }
  tlib_pass_if_true("fork", pid > 0, "fork failed");
  if (pid <= 0 || pid != waitpid(pid, &status, 0)) {
    return 0;
  }
  return WIFSIGNALED(status) ? WTERMSIG(status) : 0;
}

static void test_unreadable_stack_in_main_thread(void) {
  static const int signals[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT};
  size_t i;
  char name[128];

  for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
    snprintf(name, sizeof(name), "signal %d, unreadable PHP stack", signals[i]);
    tlib_pass_if_int_equal(name, signals[i],
                           run_child_unreadable_stack(signals[i], false));
  }
  tlib_pass_if_int_equal("hardware fault, unreadable PHP stack", SIGSEGV,
                         run_child_unreadable_stack(SIGSEGV, true));
}

#endif /* not ZTS */

void test_main(void* p NRUNUSED) {
  test_signal_is_fault();
#ifndef ZTS
  test_unreadable_stack_in_main_thread();
#endif
#ifdef ZTS
  /* Control: a thread that still has its PHP storage. */
  test_signal_in_thread("thread with PHP storage", thread_signal_in_request);
  /* A thread whose PHP storage was already freed. */
  test_signal_in_thread("thread after ts_free_thread",
                        thread_signal_after_free);
  /* A thread that PHP does not know. */
  test_signal_in_thread("thread PHP does not know", thread_signal_unknown);
  /* The handler's own read faults, for real, and the signal is still reported.
   */
  test_signal_in_thread("thread with an unreadable PHP stack",
                        thread_signal_unreadable);
  test_hardware_fault_in_thread();
  test_guard_in_this_process();
#endif
}
