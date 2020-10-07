/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "util_logging.h"
#include "util_memory.h"
#include "util_sleep.h"
#include "util_syscalls.h"
#include "util_threads.h"

#include "tlib_main.h"

typedef struct _test_threads_state_t {
  nrthread_mutex_t static_mutex;
  nrthread_mutex_t mutex;
  nrthread_mutex_t mutex1;
} test_threads_state_t;

#define SLEEP_SCALE 4

static void* test_threads_thread1(void* vp) {
  test_threads_state_t* p = (test_threads_state_t*)vp;

  (void)p;
  nr_msleep(SLEEP_SCALE * 10); /* sleep for less than thread2 sleeps to avoid
                                  output ordering issues. */
  tlib_pass_if_true("thread created", 1, "true");
  nrl_always("test_threads_thread1 created OK");
  return 0;
}

/*
 * Test 4: lock and force deadlock then unlock mutex.
 */
static void test_threads_test4(test_threads_state_t* p) {
  nr_status_t rv;

  rv = nrt_mutex_lock(&p->mutex);
  tlib_pass_if_true("dynamic mutex locked", NR_SUCCESS == rv, "rv=%d", (int)rv);
  rv = nrt_mutex_lock(&p->mutex);
  tlib_pass_if_true("dynamic relock fails with deadlock", NR_FAILURE == rv,
                    "rv=%d", (int)rv);

  rv = nrt_mutex_unlock(&p->mutex);
  tlib_pass_if_true("dynamic unlock ok", NR_SUCCESS == rv, "rv=%d", (int)rv);

  rv = nrt_mutex_lock(&p->mutex);
  tlib_pass_if_true("relock after failed unlock ok", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  rv = nrt_mutex_unlock(&p->mutex);
  tlib_pass_if_true("unlock after relock ok", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
}

static void test_static_mutex(test_threads_state_t* p) {
  nr_status_t rv;

  /*
   * acquire static lock.
   */
  rv = nrt_mutex_lock(&p->static_mutex);
  tlib_pass_if_true("acquire static mutex", NR_SUCCESS == rv, "rv=%d", (int)rv);

  /*
   * release static mutex
   */
  rv = nrt_mutex_unlock(&p->static_mutex);
  tlib_pass_if_true("mutex released", NR_SUCCESS == rv, "rv=%d", (int)rv);
  rv = nrt_mutex_lock(&p->static_mutex);
  tlib_pass_if_true("mutex acquired after release", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  rv = nrt_mutex_unlock(&p->static_mutex);
  tlib_pass_if_true("mutex release after relock ok", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
}

/*
 * The test itself is crafted to test parallelism.
 *
 * Running the test multiple times in parallel does not yet work.
 */
tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = sizeof(test_threads_state_t)};

void test_main(void* vp) {
  nr_status_t rv;
  nrthread_t t1;
  char tmpfilename[BUFSIZ];

  test_threads_state_t* p = (test_threads_state_t*)vp;
  pthread_mutex_init(&p->static_mutex, NULL);

  snprintf(tmpfilename, sizeof(tmpfilename), "./threadslog.tmp");

  /*
   * We're going to want logging for these tests
   */
  nr_unlink(tmpfilename);
  nrl_set_log_file(tmpfilename);
  nrl_set_log_level("verbosedebug");

  test_static_mutex(p);

  /*
   * Test 3: initialize a mutex (will have deadlock avoidance)
   */
  rv = nrt_mutex_init(&p->mutex, 0);
  tlib_pass_if_true("mutex init", NR_SUCCESS == rv, "rv=%d", (int)rv);

  /*
   * Test 4: lock and force deadlock then unlock mutex.
   */
  test_threads_test4(p);

  /*
   * Test 5: create a simple thread that produces a log message and exits.
   */
  rv = nrt_create(&t1, 0, test_threads_thread1, p);
  tlib_pass_if_true("simple thread create OK", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  nrt_join(t1, 0);
}
