/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_random.h"

#include "tlib_main.h"

static void test_range_bad_params(void) {
  nr_random_t* rnd = nr_random_create();

  nr_random_seed(rnd, 345345);

  tlib_pass_if_ulong_equal("NULL rnd", 0, nr_random_range(NULL, 10));
  tlib_pass_if_ulong_equal("negative max (max too large)", 0,
                           nr_random_range(rnd, -1));
  tlib_pass_if_ulong_equal("zero max", 0, nr_random_range(rnd, 0));
  tlib_pass_if_ulong_equal("one max", 0, nr_random_range(rnd, 1));
  tlib_pass_if_ulong_equal(
      "max too large", 0,
      nr_random_range(rnd, NR_RANDOM_MAX_EXCLUSIVE_LIMIT + 1));

  nr_random_destroy(&rnd);
}

static void test_range(void) {
  nr_random_t* rnd;
  unsigned long x;
  int i;
  unsigned long max_exclusive;

  rnd = nr_random_create();

  nr_random_seed(rnd, 345345);

  /*
   * Test that multiple calls with the same parameters return different values.
   * Note that this tests that we can get 0 and 9 (the min and max return
   * value).
   */
  tlib_pass_if_ulong_equal("random from range", 0, nr_random_range(rnd, 10));
  tlib_pass_if_ulong_equal("random from range", 7, nr_random_range(rnd, 10));
  tlib_pass_if_ulong_equal("random from range", 3, nr_random_range(rnd, 10));
  tlib_pass_if_ulong_equal("random from range", 5, nr_random_range(rnd, 10));
  tlib_pass_if_ulong_equal("random from range", 3, nr_random_range(rnd, 10));
  tlib_pass_if_ulong_equal("random from range", 9, nr_random_range(rnd, 10));

  /* Test the min. */
  tlib_pass_if_ulong_equal("min max_exclusive", 0, nr_random_range(rnd, 2));
  tlib_pass_if_ulong_equal("min max_exclusive", 1, nr_random_range(rnd, 2));
  tlib_pass_if_ulong_equal("min max_exclusive", 1, nr_random_range(rnd, 2));

  /* Test the max. */
  tlib_pass_if_ulong_equal("max max_exclusive", 1391330424,
                           nr_random_range(rnd, NR_RANDOM_MAX_EXCLUSIVE_LIMIT));
  tlib_pass_if_ulong_equal("max max_exclusive", 58941426,
                           nr_random_range(rnd, NR_RANDOM_MAX_EXCLUSIVE_LIMIT));
  tlib_pass_if_ulong_equal("max max_exclusive", 2045540820,
                           nr_random_range(rnd, NR_RANDOM_MAX_EXCLUSIVE_LIMIT));

  /*
   * Test that the numbers are always in range.  Here max_exclusive is chosen
   * specifically to increase code coverage since it maximizes
   * NR_RANDOM_MAX_EXCLUSIVE_LIMIT - largest_multiple
   */
  max_exclusive = (NR_RANDOM_MAX_EXCLUSIVE_LIMIT / 2) + 1;
  for (i = 0; i < 100; i++) {
    x = nr_random_range(rnd, max_exclusive);
    tlib_pass_if_true("random number in range", x < max_exclusive, "x=%ld", x);
  }

  nr_random_destroy(&rnd);
}

static void test_real(void) {
  double rv;
  nr_random_t* rnd;

  rnd = nr_random_create();
  nr_random_seed(rnd, 345345);

  rv = nr_random_real(NULL);
  tlib_pass_if_double_equal("NULL nr_random_real", -1.0, rv);

  rv = nr_random_real(rnd);
  tlib_fail_if_double_equal("nr_random_real", 0.0, rv);

  nr_random_destroy(&rnd);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_range_bad_params();
  test_range();
  test_real();
}
