/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "util_random.h"
#include "util_sampling.h"

#include "tlib_main.h"

static void test_no_random_generator(void) {
  nr_sampling_priority_t p = nr_generate_initial_priority(NULL);
  tlib_pass_if_false("NULL random number generator generates invalid priority",
                     nr_priority_is_valid(p), "p=%f", p);
}

static void test_with_random_generator(nr_random_t* rnd) {
  nr_sampling_priority_t p;

  p = nr_generate_initial_priority(rnd);
  tlib_pass_if_true(
      "well-formed random number generator generates valid initial priority",
      nr_priority_is_valid(p), "p=%f", p);
}

static void test_comparison(nr_random_t* rnd) {
  nr_sampling_priority_t p;

  p = nr_generate_initial_priority(rnd);

  tlib_pass_if_true("p is higher", nr_is_higher_priority(p, NR_PRIORITY_LOWEST),
                    "p=%f", p);
  tlib_pass_if_false("p is lower",
                     nr_is_higher_priority(p, NR_PRIORITY_HIGHEST), "p=%f", p);
  tlib_pass_if_false("p is equal to p, thus lower", nr_is_higher_priority(p, p),
                     "p=%f", p);
}

static void test_validity(void) {
  nr_sampling_priority_t p = 0.00001;
  tlib_pass_if_true("p is valid", nr_priority_is_valid(p), "p=%f", p);

  p = 0.99999;
  tlib_pass_if_true("p is valid", nr_priority_is_valid(p), "p=%f", p);

  p = NR_PRIORITY_LOWEST;
  tlib_pass_if_true("p is valid", nr_priority_is_valid(p), "p=%f", p);

  p = NR_PRIORITY_HIGHEST;
  tlib_pass_if_false("p is invalid", nr_priority_is_valid(p), "p=%f", p);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  nr_random_t* rnd;

  rnd = nr_random_create();
  nr_random_seed(rnd, 345345);

  test_no_random_generator();
  test_with_random_generator(rnd);
  test_comparison(rnd);
  test_validity();

  nr_random_destroy(&rnd);
}
