/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/socket.h>

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "util_memory.h"
#include "util_sleep.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_threads.h"

#include "tlib_main.h"

int nbsockpair(int vecs[2]) {
  int f;
  int r = socketpair(AF_LOCAL, SOCK_STREAM, 0, vecs);

  if (0 != r) {
    return r;
  }

  f = nr_fcntl(vecs[0], F_GETFL, 0);
  f |= O_NONBLOCK;
  nr_fcntl(vecs[0], F_SETFL, f);
  f = nr_fcntl(vecs[1], F_GETFL, 0);
  f |= O_NONBLOCK;
  nr_fcntl(vecs[1], F_SETFL, f);

  return r;
}

int tlib_argc = 0;
char* const* tlib_argv = NULL;
int tlib_passcount = 0;
int tlib_unexpected_failcount = 0;

const char* progname;
static int ignore_unexpected_failures = 0;

/*
 * worker_parallelism corresponds to the -j flag.
 *
 * Values of <  0 will run sequentially with iterations given by abs value
 * Values of == 0 will run in parallel using test's suggested parallelism
 * Values of >  0 will run in parallel use that many parallel workers
 */
static int worker_parallelism = -1;

static void sig_handler(int sig) {
  printf("\n\n>>> %s: SIGNAL %d received!\n\n", progname, sig);
  tlib_unexpected_failcount++;

  printf("%24s: %6d of %6d tests passed, and %d failed\n", progname,
         tlib_passcount, tlib_passcount + tlib_unexpected_failcount,
         tlib_unexpected_failcount);
  exit(1);
}

static void usage(void) {
  fprintf(stderr, "%s [-U] [-j parallel]\n", progname);
  fprintf(
      stderr,
      "-U\tIgnore unexpected failures when computing process return code\n");
  exit(1);
}

static void consume_args(int argc, char* const argv[]) {
  int opt;

  progname = nr_strrchr(argv[0], '/');
  if (progname) {
    progname++;
  } else {
    progname = argv[0];
  }

  while ((opt = getopt(argc, argv, "UEr:c:w:j:")) != -1) {
    switch (opt) {
      case 'U': /* Ignore unexpected failures when computing process return
                   code. */
        ignore_unexpected_failures = 1;
        break;

      case 'j':
        worker_parallelism = (int)strtol(optarg, 0, 10);
        break;

      default:
        usage();
        /* NOTREACHED */
    }
  }
}

void tlib_ignore_sigpipe(void) {
  int rv;
  struct sigaction sa;

  nr_memset(&sa, 0, sizeof(sa));

  sa.sa_handler = SIG_IGN;
  sa.sa_flags = SA_RESTART;

  rv = sigaction(SIGPIPE, &sa, NULL);
  tlib_pass_if_true("sigpipe ignored", 0 == rv, "rv=%d", rv);
}

void test_main_parallel_driver(int suggested_nthreads, size_t state_size);

int main(int argc, char* const argv[]) {
  struct sigaction sa;
  int failures_contributing_to_return_code;

  int i;
  int return_value = 0;

  tlib_argc = argc;
  tlib_argv = (char* const*)argv;
  consume_args(argc, argv);

  nr_memset(&sa, 0, sizeof(sa));

  sa.sa_handler = sig_handler;
  sigfillset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, 0);
  sigaction(SIGBUS, &sa, 0);
  sigaction(SIGFPE, &sa, 0);
  sigaction(SIGILL, &sa, 0);
  sigaction(SIGABRT, &sa, 0);

  test_main_parallel_driver(parallel_info.suggested_nthreads,
                            parallel_info.state_size);

  if (0 == tlib_unexpected_failcount) {
    printf("%30s: all %6d tests passed", progname, tlib_passcount);
    return_value = 0;
  } else {
    printf("%30s: %6d of %6d tests passed, %6d failed", progname,
           tlib_passcount, tlib_passcount + tlib_unexpected_failcount,
           tlib_unexpected_failcount);
    failures_contributing_to_return_code = 0;
    if (!ignore_unexpected_failures && tlib_unexpected_failcount > 0) {
      failures_contributing_to_return_code += tlib_unexpected_failcount;
    }
    return_value = (failures_contributing_to_return_code > 0) ? 1 : 0;
  }

  printf(" ");
  for (i = 0; i < argc; i++) {
    printf(" %s", argv[i] ? argv[i] : "<null>");
  }
  printf("\n");

  return return_value;
}

void test_obj_as_json_fn(const char* testname,
                         const nrobj_t* obj,
                         const char* expected_json,
                         const char* file,
                         int line) {
  char* json = nro_to_json(obj);

  test_pass_if_true(testname, 0 == nr_strcmp(expected_json, json),
                    "expected_json=%s json=%s", expected_json, json);

  nr_free(json);
}

pthread_key_t thread_id_key;

void* tlib_getspecific(void) {
  return pthread_getspecific(thread_id_key);
}

static void* test_main_parallel_driver_helper(void* p) {
  pthread_setspecific(thread_id_key, p);
  test_main(p);
  return NULL;
}

void test_main_parallel_driver(int suggested_nthreads, size_t state_size) {
  nr_status_t rv;
  nrthread_t* tid = 0;
  void** state = 0; /* array of pointers to client state */
  int i;
  int o;
  pthread_attr_t attr;
  int force_sequential = 0;
  int par_outer = 1;
  int par_inner = 1;
  int nthreads = 0;

  pthread_key_create(&thread_id_key, NULL);

  if (suggested_nthreads <= 0) {
    worker_parallelism = suggested_nthreads;
  }

  if (worker_parallelism < 0) {
    force_sequential = 1;
    nthreads = -worker_parallelism;
  } else if (worker_parallelism == 0) {
    force_sequential = 0;
    if (suggested_nthreads <= 0) {
      suggested_nthreads = 1;
    }
    nthreads = suggested_nthreads;
  } else {
    force_sequential = 0;
    nthreads = worker_parallelism;
  }

  par_outer = force_sequential ? nthreads : 1;
  par_inner = force_sequential ? 1 : nthreads;

  tid = (nrthread_t*)nr_calloc(nthreads, sizeof(tid[0]));
  state = (void**)nr_calloc(nthreads, sizeof(state[0]));
  for (i = 0; i < nthreads; i++) {
    state[i] = nr_calloc(1, state_size);
  }

  if (force_sequential) {
    for (i = 0; i < nthreads; i++) {
      test_main_parallel_driver_helper(state[i]);
    }
  } else {
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1 << 20);

    for (o = 0; o < par_outer; o++) {
      for (i = 0; i < par_inner; i++) {
        rv = nrt_create(&tid[i], &attr, test_main_parallel_driver_helper,
                        state[i]);
        tlib_pass_if_true("thread create OK", NR_SUCCESS == rv, "rv=%d",
                          (int)rv);
      }

      for (i = 0; i < par_inner; i++) {
        rv = nrt_join(tid[i], 0);
        tlib_pass_if_true("thread join OK", NR_SUCCESS == rv, "rv=%d", (int)rv);
      }
    }
  }

  for (i = 0; i < nthreads; i++) {
    nr_free(state[i]);
  }

  nr_free(state);
  nr_free(tid);
}
