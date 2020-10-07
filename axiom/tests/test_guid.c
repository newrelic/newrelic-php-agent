/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_guid.h"
#include "util_memory.h"

#include "tlib_main.h"

static void test_create(void) {
  char* guid;
  nr_random_t* rnd = nr_random_create();

  nr_random_seed(rnd, 345345);

  guid = nr_guid_create(NULL);
  tlib_pass_if_str_equal("NULL random", guid, "0000000000000000");
  nr_free(guid);

  guid = nr_guid_create(rnd);
  tlib_pass_if_str_equal("guid creation", guid, "078ad44c1960eab7");
  nr_free(guid);

  guid = nr_guid_create(rnd);
  tlib_pass_if_str_equal("repeat guid creation", guid, "11da3087c4400533");
  nr_free(guid);

  nr_random_destroy(&rnd);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create();
}
