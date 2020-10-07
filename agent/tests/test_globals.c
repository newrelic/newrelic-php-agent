/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_globals.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_init(void) {
  nrphpglobals_t zeroed;

  // Basically, we'll set the per process globals to junk, call init, and
  // verify that they're all 0.
  nr_memset(&nr_php_per_process_globals, 42, sizeof(nrphpglobals_t));
  nr_memset(&zeroed, 0, sizeof(nrphpglobals_t));

  nr_php_global_init();

  tlib_pass_if_int_equal(
      "all bytes are zero", 0,
      nr_memcmp(&zeroed, &nr_php_per_process_globals, sizeof(nrphpglobals_t)));

  nr_php_global_destroy();
}

static int once_called = 0;

static void increment_once_called(void) {
  ++once_called;
}

static void test_once(void) {
  nr_php_global_init();
  once_called = 0;

  nr_php_global_once(increment_once_called);
  nr_php_global_once(increment_once_called);

  tlib_pass_if_int_equal("check once called state", 1, once_called);

  nr_php_global_destroy();

  nr_php_global_init();
  once_called = 0;

  nr_php_global_once(increment_once_called);
  nr_php_global_once(increment_once_called);

  tlib_pass_if_int_equal("check once called state", 1, once_called);

  nr_php_global_destroy();
}

void test_main(void* p NRUNUSED) {
  test_init();
  test_once();
}
