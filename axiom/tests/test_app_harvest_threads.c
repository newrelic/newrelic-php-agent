/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Multi-threaded test verifying per-thread adaptive sampling independence.
 *
 * Background
 * ----------
 * In ZTS builds (e.g. FrankenPHP worker mode), multiple threads share a single
 * nrapp_t.  Each thread maintains its own nr_app_harvest_stats_t entry in
 * app->harvest_map, keyed by pthread_self().  This ensures each thread makes
 * independent sampling decisions targeting adaptive_sampling_config.
 * target_transactions_per_cycle transactions per harvest cycle, rather than
 * all threads competing for one shared quota.
 *
 * What this test verifies
 * -----------------------
 * 1. Per-thread independence: each thread's transaction counters (transactions_
 *    seen, transactions_sampled) accumulate independently.  If threads shared
 *    one entry the counts would be wrong.
 *
 * 2. Config reset propagation: nr_app_update_harvest_config() iterates every
 *    map entry via nr_hashmap_apply() and resets all per-thread stats when
 *    connect_timestamp or frequency changes (simulating a daemon reconnect).
 *    Phase 2 verifies that every thread's entry was reset and that each thread
 *    independently hits the target again from a clean state.
 *
 * Test structure
 * --------------
 * NUM_THREADS real pthreads share one nrapp_t.  Each thread:
 *
 *   Phase 1: make CALLS_PER_PHASE sampling decisions.
 *            In the first harvest cycle the first SAMPLING_TARGET transactions
 *            are always sampled and the rest never are — deterministic.
 *            Expected: seen=CALLS_PER_PHASE, sampled=SAMPLING_TARGET.
 *
 *   Barrier: all threads wait while the main thread calls
 *            nr_app_update_harvest_config() with a new connect_timestamp,
 *            resetting every per-thread entry in the map.
 *
 *   Phase 2: another CALLS_PER_PHASE decisions from a clean slate.
 *            Expected: seen=CALLS_PER_PHASE, sampled=SAMPLING_TARGET again.
 *
 * Why SAMPLING_TARGET < CALLS_PER_PHASE
 * --------------------------------------
 * The first-harvest algorithm samples exactly the first SAMPLING_TARGET
 * transactions unconditionally, then rejects the rest.  Setting
 * CALLS_PER_PHASE > SAMPLING_TARGET ensures both the "always sample" and
 * "never sample" branches are exercised each phase.
 *
 * Locking
 * -------
 * Each sampling call acquires app_lock, matching the production code path
 * in nr_txn_begin().  The barrier ensures the config reset (also under
 * app_lock) happens only after all threads have finished phase 1.
 */

#include "nr_axiom.h"

#include <pthread.h>
#include <unistd.h>

#include "nr_app.h"
#include "nr_app_harvest.h"
#include "util_memory.h"
#include "util_sleep.h"
#include "util_threads.h"

#include "tlib_main.h"

#define NUM_THREADS     4
#define CALLS_PER_PHASE 10
#define SAMPLING_TARGET 5

typedef struct {
  nrapp_t*        app;
  nr_random_t*    rnd;   /* shared; safe because app_lock serialises all calls */

  /* barrier: threads wait here between phases while main resets config */
  pthread_mutex_t barrier_mutex;
  pthread_cond_t  barrier_cond;
  int             threads_waiting;
  int             config_updated;

  /* per-thread results recorded after each phase, indexed by thread_idx */
  uint64_t        seen_phase1[NUM_THREADS];
  uint64_t        sampled_phase1[NUM_THREADS];
  uint64_t        seen_after_reset[NUM_THREADS];   /* must be 0 after config reset */
  uint64_t        sampled_after_reset[NUM_THREADS];
  uint64_t        seen_phase2[NUM_THREADS];
  uint64_t        sampled_phase2[NUM_THREADS];

  /* assigns each thread a stable index into the result arrays */
  pthread_mutex_t idx_mutex;
  int             next_idx;
} sampling_thread_state_t;

static void* sampling_thread(void* arg) {
  sampling_thread_state_t* s = (sampling_thread_state_t*)arg;
  nr_app_harvest_stats_t* th;
  pthread_t tid;
  int idx;

  /* claim a stable result-array index before any other thread can */
  pthread_mutex_lock(&s->idx_mutex);
  idx = s->next_idx++;
  pthread_mutex_unlock(&s->idx_mutex);

  /*
   * Create this thread's map entry under app_lock.  pthread_self() is the
   * key — unique per thread, stable for the thread's lifetime.
   */
  nrt_mutex_lock(&s->app->app_lock);
  tid = pthread_self();
  th = nr_app_get_or_create_thread_harvest(s->app, (uint64_t)(uintptr_t)tid);
  nrt_mutex_unlock(&s->app->app_lock);

  /* --- Phase 1 ---------------------------------------------------------- */
  for (int i = 0; i < CALLS_PER_PHASE; i++) {
    nrt_mutex_lock(&s->app->app_lock);
    nr_app_harvest_should_sample(&s->app->adaptive_sampling_config, th, s->rnd);
    nrt_mutex_unlock(&s->app->app_lock);
  }

  /* record counters before the barrier resets them */
  s->seen_phase1[idx]    = th->transactions_seen;
  s->sampled_phase1[idx] = th->transactions_sampled;

  /*
   * Barrier: signal the main thread that this thread is done with phase 1,
   * then wait until the main thread has updated the config.
   */
  pthread_mutex_lock(&s->barrier_mutex);
  s->threads_waiting++;
  pthread_cond_broadcast(&s->barrier_cond);
  while (!s->config_updated) {
    pthread_cond_wait(&s->barrier_cond, &s->barrier_mutex);
  }
  pthread_mutex_unlock(&s->barrier_mutex);

  /* --- Phase 2 ---------------------------------------------------------- */
  /*
   * th still points to this thread's map entry — nr_app_update_harvest_config
   * resets the entry's stats in place (does not delete or reallocate it).
   * Record the stats immediately after the reset to verify they are zeroed.
   */
  s->seen_after_reset[idx]    = th->transactions_seen;
  s->sampled_after_reset[idx] = th->transactions_sampled;

  for (int i = 0; i < CALLS_PER_PHASE; i++) {
    nrt_mutex_lock(&s->app->app_lock);
    nr_app_harvest_should_sample(&s->app->adaptive_sampling_config, th, s->rnd);
    nrt_mutex_unlock(&s->app->app_lock);
  }

  s->seen_phase2[idx]    = th->transactions_seen;
  s->sampled_phase2[idx] = th->transactions_sampled;

  return NULL;
}

static void test_per_thread_sampling_independence(void) {
  sampling_thread_state_t s = {0};
  nrapp_t app = {0};
  nrthread_t threads[NUM_THREADS];
  /*
   * Capture now once.  Phase 1 config uses connect_timestamp=now so that
   * nr_app_harvest_stats_init() places next_harvest in the future (within the
   * first harvest cycle).  Phase 2 config uses connect_timestamp=now-1s —
   * different from now, which triggers the change-detection reset, but still
   * within the 60-second first harvest window so sampling behaviour is the
   * same deterministic first-harvest algorithm.
   */
  nrtime_t now = nr_get_time();

  nrt_mutex_init(&app.app_lock, 0);
  app.harvest_map
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_app_harvest_stats_dtor);
  nr_app_update_harvest_config(&app, now, 60 * NR_TIME_DIVISOR, SAMPLING_TARGET);

  s.app = &app;
  s.rnd = nr_random_create();
  nr_random_seed(s.rnd, 12345);
  pthread_mutex_init(&s.barrier_mutex, NULL);
  pthread_cond_init(&s.barrier_cond, NULL);
  pthread_mutex_init(&s.idx_mutex, NULL);

  /* spawn all threads; they begin phase 1 immediately */
  for (int i = 0; i < NUM_THREADS; i++) {
    nrt_create(&threads[i], NULL, sampling_thread, &s);
  }

  /* wait until every thread has finished phase 1 and is at the barrier */
  pthread_mutex_lock(&s.barrier_mutex);
  while (s.threads_waiting < NUM_THREADS) {
    pthread_cond_wait(&s.barrier_cond, &s.barrier_mutex);
  }
  pthread_mutex_unlock(&s.barrier_mutex);

  /*
   * Simulate a daemon reconnect: new connect_timestamp triggers
   * nr_app_update_harvest_config() to iterate harvest_map via
   * nr_hashmap_apply() and call nr_app_harvest_stats_init() on every entry,
   * resetting all threads' stats simultaneously.
   */
  nrt_mutex_lock(&app.app_lock);
  nr_app_update_harvest_config(&app, now - NR_TIME_DIVISOR,
                                60 * NR_TIME_DIVISOR, SAMPLING_TARGET);
  nrt_mutex_unlock(&app.app_lock);

  /* release all threads to run phase 2 */
  pthread_mutex_lock(&s.barrier_mutex);
  s.config_updated = 1;
  pthread_cond_broadcast(&s.barrier_cond);
  pthread_mutex_unlock(&s.barrier_mutex);

  for (int i = 0; i < NUM_THREADS; i++) {
    nrt_join(threads[i], NULL);
  }

  /*
   * Each thread must show exactly CALLS_PER_PHASE seen and SAMPLING_TARGET
   * sampled.  If threads shared one entry, some threads would observe
   * transactions_seen > CALLS_PER_PHASE (another thread's increments) and
   * transactions_sampled < SAMPLING_TARGET (quota used up by other threads).
   */
  for (int i = 0; i < NUM_THREADS; i++) {
    tlib_pass_if_uint64_t_equal("phase 1: transactions_seen per thread",
                                CALLS_PER_PHASE, s.seen_phase1[i]);
    tlib_pass_if_uint64_t_equal("phase 1: transactions_sampled per thread",
                                SAMPLING_TARGET, s.sampled_phase1[i]);
  }

  /* verify config reset zeroed all per-thread stats */
  for (int i = 0; i < NUM_THREADS; i++) {
    tlib_pass_if_uint64_t_equal("stats reset: transactions_seen zeroed", 0,
                                s.seen_after_reset[i]);
    tlib_pass_if_uint64_t_equal("stats reset: transactions_sampled zeroed", 0,
                                s.sampled_after_reset[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    tlib_pass_if_uint64_t_equal("phase 2: transactions_seen per thread",
                                CALLS_PER_PHASE, s.seen_phase2[i]);
    tlib_pass_if_uint64_t_equal("phase 2: transactions_sampled per thread",
                                SAMPLING_TARGET, s.sampled_phase2[i]);
  }

  nr_random_destroy(&s.rnd);
  pthread_mutex_destroy(&s.barrier_mutex);
  pthread_cond_destroy(&s.barrier_cond);
  pthread_mutex_destroy(&s.idx_mutex);
  nr_hashmap_destroy(&app.harvest_map);
  nrt_mutex_destroy(&app.app_lock);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1,
                                      .state_size = 0};

void test_main(void* p NRUNUSED) {
  /* kill the process if the test deadlocks or hangs */
  alarm(10);
  test_per_thread_sampling_independence();
  alarm(0);
}
