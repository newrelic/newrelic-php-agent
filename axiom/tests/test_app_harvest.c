/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_app.h"
#include "nr_app_harvest.h"
#include "nr_app_harvest_private.h"
#include "util_threads.h"

#include "tlib_main.h"


static void test_update_harvest_config(void) {
  nrapp_t app = {0};
  nr_app_harvest_stats_t* stats;
  const uint64_t key = 42;

  nrt_mutex_init(&app.app_lock, 0);
  app.harvest_map
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_app_harvest_stats_dtor);

  /*
   * Test : NULL app — should not crash.
   */
  nr_app_update_harvest_config(NULL, 1, 60, 10);

  /*
   * Pre-populate one thread's stats entry with non-zero counters.
   */
  stats = nr_app_get_or_create_thread_harvest(&app, key);
  stats->transactions_seen = 5;
  stats->transactions_sampled = 3;

  /*
   * Test : New connection; config change should reset thread stats.
   */
  nr_app_update_harvest_config(&app, 1, 60, 10);
  tlib_pass_if_uint64_t_equal("connect timestamp", 1,
                              app.adaptive_sampling_config.connect_timestamp);
  tlib_pass_if_uint64_t_equal("frequency", 60,
                              app.adaptive_sampling_config.frequency);
  tlib_pass_if_uint64_t_equal("target", 10,
                              app.adaptive_sampling_config.target_transactions_per_cycle);
  stats = nr_app_get_or_create_thread_harvest(&app, key);
  tlib_pass_if_uint64_t_equal("transactions seen reset", 0,
                              stats->transactions_seen);
  tlib_pass_if_uint64_t_equal("transactions sampled reset", 0,
                              stats->transactions_sampled);
  tlib_pass_if_uint64_t_equal("prev transactions seen reset", 0,
                              stats->prev_transactions_seen);
  tlib_pass_if_true("next harvest set", stats->next_harvest > 0,
                    "next_harvest=%" PRIu64, stats->next_harvest);

  /*
   * Test : Same connection; unchanged config should not reset thread stats.
   */
  stats->transactions_seen = 1;
  stats->transactions_sampled = 2;
  nr_app_update_harvest_config(&app, 1, 60, 10);
  stats = nr_app_get_or_create_thread_harvest(&app, key);
  tlib_pass_if_uint64_t_equal("transactions seen preserved", 1,
                              stats->transactions_seen);
  tlib_pass_if_uint64_t_equal("transactions sampled preserved", 2,
                              stats->transactions_sampled);

  nr_hashmap_destroy(&app.harvest_map);
  nrt_mutex_destroy(&app.app_lock);
}

static void test_stats_init(void) {
  nr_app_harvest_config_t cfg = {.connect_timestamp = 100, .frequency = 60,
                                 .target_transactions_per_cycle = 10};
  nr_app_harvest_stats_t ah = {.transactions_seen = 5, .transactions_sampled = 3,
                               .threshold = 7};

  /*
   * Test : Bad parameters.
   */
  nr_app_harvest_stats_init(NULL, &ah);
  tlib_pass_if_uint64_t_equal("NULL cfg: ah unchanged", 5, ah.transactions_seen);

  nr_app_harvest_stats_init(&cfg, NULL);

  /*
   * Test : Normal init resets mutable counters.
   */
  nr_app_harvest_stats_init(&cfg, &ah);
  tlib_pass_if_uint64_t_equal("threshold reset", 0, ah.threshold);
  tlib_pass_if_uint64_t_equal("prev seen reset", 0, ah.prev_transactions_seen);
  tlib_pass_if_uint64_t_equal("seen reset", 0, ah.transactions_seen);
  tlib_pass_if_uint64_t_equal("sampled reset", 0, ah.transactions_sampled);
  tlib_pass_if_true("next_harvest set", ah.next_harvest > 0,
                    "expected next_harvest > 0, got " NR_TIME_FMT,
                    ah.next_harvest);
}

static void test_calculate_next_harvest_time(void) {
  nr_app_harvest_config_t cfg = {.connect_timestamp = 100, .frequency = 0};

  /*
   * Test : NULL cfg.
   */
  tlib_pass_if_uint64_t_equal(
      "NULL cfg", 0,
      nr_app_harvest_calculate_next_harvest_time(NULL, 0));

  /*
   * Test : Division by zero.
   */
  tlib_pass_if_uint64_t_equal(
      "zero frequency", 100,
      nr_app_harvest_calculate_next_harvest_time(&cfg, 0));

  /*
   * Test : Time travel.
   */
  cfg.frequency = 60;
  tlib_pass_if_uint64_t_equal(
      "clock skew", 100, nr_app_harvest_calculate_next_harvest_time(&cfg, 0));

  /*
   * Test : Exactly equal to the connect timestamp.
   */
  tlib_pass_if_uint64_t_equal(
      "connect time", 160,
      nr_app_harvest_calculate_next_harvest_time(&cfg, 100));

  /*
   * Test : In the middle of a harvest cycle.
   */
  tlib_pass_if_uint64_t_equal(
      "mid cycle", 160, nr_app_harvest_calculate_next_harvest_time(&cfg, 130));

  /*
   * Test : At the exact end/start of a harvest cycle.
   */
  tlib_pass_if_uint64_t_equal(
      "end cycle", 220, nr_app_harvest_calculate_next_harvest_time(&cfg, 160));
}

static void test_calculate_threshold(void) {
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_uint64_t_equal("0 target", 0,
                              nr_app_harvest_calculate_threshold(0, 10));

  /*
   * Test : Zero sampled.
   */
  tlib_pass_if_uint64_t_equal("0 seen", 0,
                              nr_app_harvest_calculate_threshold(10, 0));
  /*
   * Test : Normal operation.
   */
  tlib_pass_if_uint64_t_equal("target > seen", 0,
                              nr_app_harvest_calculate_threshold(10, 5));
  tlib_pass_if_uint64_t_equal("target == seen", 6,
                              nr_app_harvest_calculate_threshold(10, 10));
  tlib_pass_if_uint64_t_equal("target < seen", 0,
                              nr_app_harvest_calculate_threshold(10, 20));
}

static void test_is_first(void) {
  nr_app_harvest_config_t cfg = {.connect_timestamp = 100, .frequency = 60};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("NULL cfg", false,
                          nr_app_harvest_is_first(NULL, 0));
  /*
   * Test : Normal operation.
   */
  tlib_pass_if_bool_equal("First harvest", true,
                          nr_app_harvest_is_first(&cfg, 111));
  tlib_pass_if_bool_equal("Still first harvest", true,
                          nr_app_harvest_is_first(&cfg, 112));
  tlib_pass_if_bool_equal("Second harvest", false,
                          nr_app_harvest_is_first(&cfg, 161));
  tlib_pass_if_bool_equal("Third harvest", false,
                          nr_app_harvest_is_first(&cfg, 222));
}

static void test_should_sample(nr_random_t* rnd) {
  const uint64_t target = 10;
  nr_app_harvest_config_t cfg = {.connect_timestamp = 100,
                                 .frequency = 60,
                                 .target_transactions_per_cycle = target};
  nr_app_harvest_stats_t ah = {0};
  uint64_t i;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal(
      "NULL cfg", false,
      nr_app_harvest_private_should_sample(NULL, &ah, rnd, 111));
  tlib_pass_if_bool_equal(
      "NULL ah", false,
      nr_app_harvest_private_should_sample(&cfg, NULL, rnd, 111));
  tlib_pass_if_bool_equal(
      "NULL rnd", false,
      nr_app_harvest_private_should_sample(&cfg, &ah, NULL, 111));

  /*
   * Test : First harvest.
   *
   * We'll put through ten transactions, all of which should be sampled, and
   * another ten, all of which should be ignored.
   */
  for (i = 0; i < target; i++) {
    tlib_pass_if_bool_equal(
        "first harvest; first ten transactions", true,
        nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 111));
    tlib_pass_if_uint64_t_equal("seen", i + 1, ah.transactions_seen);
    tlib_pass_if_uint64_t_equal("sampled", i + 1, ah.transactions_sampled);
  }

  for (i = 0; i < target; i++) {
    tlib_pass_if_bool_equal(
        "first harvest; next ten transactions", false,
        nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 111));
    tlib_pass_if_uint64_t_equal("seen", i + target + 1, ah.transactions_seen);
    tlib_pass_if_uint64_t_equal("sampled", target, ah.transactions_sampled);
  }

  /*
   * Test : Subsequent harvest.
   *
   * Sample one more transaction in the next harvest cycle and affirm that
   * the number of transactions "seen" in the last harvest was 20.
   */
  nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 171);
  tlib_pass_if_uint64_t_equal("previously seen", 20, ah.prev_transactions_seen);
}

static void test_should_sample_subsequent_harvest(nr_random_t* rnd) {
  const uint64_t target = 10;
  nr_app_harvest_config_t cfg = {.connect_timestamp = 100,
                                 .frequency = 60,
                                 .target_transactions_per_cycle = target};
  nr_app_harvest_stats_t ah = {0};
  uint64_t i;

  /*
   * Test : Subsequent harvest.
   *
   * In this scenario, the first harvest had 0 transactions, i.e.,
   * 0 transactions were seen in the previous harvest.  With 0
   * previous seen, the first 10 transactions are guaranteed
   * to be sampled.
   */
  for (i = 0; i < (target); i++) {
    tlib_pass_if_bool_equal(
        "subsequent harvest; first ten transactions", true,
        nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 171));
    tlib_pass_if_uint64_t_equal("previously seen", 0,
                                ah.prev_transactions_seen);
    tlib_pass_if_uint64_t_equal("threshold", 0, ah.threshold);
    tlib_pass_if_uint64_t_equal("seen", i + 1, ah.transactions_seen);
    tlib_pass_if_uint64_t_equal("sampled", i + 1, ah.transactions_sampled);
  }

  /*
   * After sampling the target number, the adaptive sampling algorithm
   * uses the threshold value to randomly determine -- with exponential
   * back-off -- whether or not to sample the transaction.  While
   * the number of sampled transactions can't be predicted, it can be
   * affirmed that the threshold is correctly recalculated and updated.
   * Moreover, it can be affirmed that the number seen is updated. */
  nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 171);
  tlib_pass_if_uint64_t_equal("threshold", 6, ah.threshold);
  tlib_pass_if_uint64_t_equal("seen", target + 1, ah.transactions_seen);

  nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 171);
  tlib_pass_if_uint64_t_equal("threshold", 6, ah.threshold);
  tlib_pass_if_uint64_t_equal("seen", target + 2, ah.transactions_seen);

  nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 171);
  tlib_pass_if_uint64_t_equal("threshold", 4, ah.threshold);
  tlib_pass_if_uint64_t_equal("seen", target + 3, ah.transactions_seen);
}

static void test_should_sample_skip_harvest(nr_random_t* rnd) {
  const uint64_t target = 10;
  nr_app_harvest_config_t cfg = {.connect_timestamp = 100,
                                 .frequency = 60,
                                 .target_transactions_per_cycle = target};
  nr_app_harvest_stats_t ah = {0};
  uint64_t i;

  /*
   * Test : First harvest.
   *
   * We'll put through ten transactions, all of which should be sampled, and
   * another ten, all of which should be ignored.
   */
  for (i = 0; i < target; i++) {
    tlib_pass_if_bool_equal(
        "first harvest; first ten transactions", true,
        nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 111));
    tlib_pass_if_uint64_t_equal("seen", i + 1, ah.transactions_seen);
    tlib_pass_if_uint64_t_equal("sampled", i + 1, ah.transactions_sampled);
  }

  /*
   * Test : Skip harvests.
   *
   * More than one harvest later, affirm that the previous number of
   * transactions seen is correctly updated to 0. */
  nr_app_harvest_private_should_sample(&cfg, &ah, rnd, 300);
  tlib_pass_if_uint64_t_equal("previous seen", 0, ah.prev_transactions_seen);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  nr_random_t* rnd;

  rnd = nr_random_create();
  nr_random_seed(rnd, 345345);

  test_update_harvest_config();
  test_stats_init();
  test_calculate_next_harvest_time();
  test_calculate_threshold();
  test_is_first();
  test_should_sample(rnd);
  test_should_sample_subsequent_harvest(rnd);
  test_should_sample_skip_harvest(rnd);
  nr_random_destroy(&rnd);
}
