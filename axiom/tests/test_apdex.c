/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "util_apdex.h"

#include "tlib_main.h"

#define tlib_pass_if_apdex_zone_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_int_equal(M, 0, (EXPECTED) == (ACTUAL))

static void test_apdex_zone(void) {
  tlib_pass_if_apdex_zone_equal("satisfying", NR_APDEX_SATISFYING,
                                nr_apdex_zone(10, 1));
  tlib_pass_if_apdex_zone_equal("satisfying", NR_APDEX_SATISFYING,
                                nr_apdex_zone(10, 10));
  tlib_pass_if_apdex_zone_equal("tolerating", NR_APDEX_TOLERATING,
                                nr_apdex_zone(10, 11));
  tlib_pass_if_apdex_zone_equal("tolerating", NR_APDEX_TOLERATING,
                                nr_apdex_zone(10, 40));
  tlib_pass_if_apdex_zone_equal("failing", NR_APDEX_FAILING,
                                nr_apdex_zone(10, 41));
  tlib_pass_if_apdex_zone_equal("failing", NR_APDEX_FAILING,
                                nr_apdex_zone(10, 100));
}

static void test_apdex_zone_label(void) {
  tlib_pass_if_char_equal("satisfying", 'S',
                          nr_apdex_zone_label(NR_APDEX_SATISFYING));
  tlib_pass_if_char_equal("tolerating", 'T',
                          nr_apdex_zone_label(NR_APDEX_TOLERATING));
  tlib_pass_if_char_equal("failing", 'F',
                          nr_apdex_zone_label(NR_APDEX_FAILING));
  tlib_pass_if_char_equal("unknown", '?',
                          nr_apdex_zone_label((nr_apdex_zone_t)0));
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_apdex_zone();
  test_apdex_zone_label();
}
