/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_math.h"

#include "tlib_main.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

static void test_log2_32(void) {
  short i;

  for (i = 0; i < 32; i++) {
    uint32_t value = (1 << i);

    tlib_pass_if_uint32_t_equal("exact value", i, nr_log2_32(value));

    if (i > 0) {
      tlib_pass_if_uint32_t_equal("one less", i - 1, nr_log2_32(value - 1));
      if (i > 1) {
        tlib_pass_if_uint32_t_equal("one more", i, nr_log2_32(value + 1));
      }
    }
  }
}

static void test_log2_64(void) {
  short i;

  for (i = 0; i < 64; i++) {
    uint64_t value = ((uint64_t)1 << i);

    tlib_pass_if_uint64_t_equal("exact value", i, nr_log2_64(value));

    if (i > 0) {
      tlib_pass_if_uint64_t_equal("one less", i - 1, nr_log2_64(value - 1));
      if (i > 1) {
        tlib_pass_if_uint64_t_equal("one more", i, nr_log2_64(value + 1));
      }
    }
  }
}

void test_main(void* p NRUNUSED) {
  test_log2_32();
  test_log2_64();
}
