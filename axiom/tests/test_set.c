/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_set.h"

#include "tlib_main.h"

#include <stdio.h>

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

static void test_bad_parameters(void) {
  nr_set_t* set = NULL;

  nr_set_destroy(NULL);
  nr_set_destroy(&set);

  tlib_pass_if_bool_equal("NULL contains", false, nr_set_contains(NULL, NULL));

  nr_set_insert(NULL, NULL);

  tlib_pass_if_size_t_equal("NULL size", 0, nr_set_size(NULL));
}

static void test_create_destroy(void) {
  nr_set_t* set;

  /*
   * Test : Destroying an empty set.
   */
  set = nr_set_create();
  tlib_pass_if_not_null("create", set);
  nr_set_destroy(&set);

  /*
   * Test : Destroying a non-empty set.
   */
  set = nr_set_create();
  tlib_pass_if_not_null("create", set);
  nr_set_insert(set, (const void*)1);
  nr_set_destroy(&set);
}

#define SET_SIZE 100000
static void test_set(void) {
  uintptr_t i;
  nr_set_t* set = nr_set_create();

  /* Insert initial values. */
  for (i = 0; i < SET_SIZE; i++) {
    nr_set_insert(set, (const void*)i);
  }

  /* Insert the same values again. */
  for (i = 0; i < SET_SIZE; i++) {
    nr_set_insert(set, (const void*)i);
  }

  /* Verify that the duplicate values weren't added. */
  tlib_pass_if_size_t_equal("set size", SET_SIZE, nr_set_size(set));

  /* Test that the expected values all exist. */
  for (i = 0; i < SET_SIZE; i++) {
    tlib_pass_if_bool_equal("exists", true,
                            nr_set_contains(set, (const void*)i));
  }

  /* Test that unexpected values don't exist. */
  for (i = SET_SIZE; i < SET_SIZE * 2; i++) {
    tlib_pass_if_bool_equal("doesn't exist", false,
                            nr_set_contains(set, (const void*)i));
  }

  nr_set_destroy(&set);
}

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_create_destroy();
  test_set();
}
