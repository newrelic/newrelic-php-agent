/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_main.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_globals.h"
#include "php_error.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_error_get_priority() {
  /*
   * Test : Unknown type.
   */
  tlib_pass_if_int_equal("Unknown error type", 20,
                         nr_php_error_get_priority(-1));
  tlib_pass_if_int_equal("Unknown error type", 20,
                         nr_php_error_get_priority(3));

  /*
   * Test : Normal operation.
   */

  tlib_pass_if_int_equal("Known error type", 0,
                         nr_php_error_get_priority(E_NOTICE));
  tlib_pass_if_int_equal("Known error type", 0,
                         nr_php_error_get_priority(E_USER_NOTICE));
  tlib_pass_if_int_equal("Known error type", 10,
                         nr_php_error_get_priority(E_STRICT));
  tlib_pass_if_int_equal("Known error type", 30,
                         nr_php_error_get_priority(E_USER_DEPRECATED));
  tlib_pass_if_int_equal("Known error type", 30,
                         nr_php_error_get_priority(E_DEPRECATED));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_USER_WARNING));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_WARNING));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_CORE_WARNING));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_COMPILE_WARNING));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_RECOVERABLE_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_USER_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_CORE_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_COMPILE_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_PARSE));
}

static void test_get_error_type_string() {
  /*
   * Test : Unknown type.
   */
  tlib_pass_if_str_equal("Unknown error type", "Error",
                         nr_get_error_type_string(-1));
  tlib_pass_if_str_equal("Unknown error type", "Error",
                         nr_get_error_type_string(3));

  /*
   * Test : Normal operation.
   */

  tlib_pass_if_str_equal("Known error type", "E_NOTICE",
                         nr_get_error_type_string(E_NOTICE));
  tlib_pass_if_str_equal("Known error type", "E_USER_NOTICE",
                         nr_get_error_type_string(E_USER_NOTICE));
  tlib_pass_if_str_equal("Known error type", "E_STRICT",
                         nr_get_error_type_string(E_STRICT));
  tlib_pass_if_str_equal("Known error type", "E_USER_NOTICE",
                         nr_get_error_type_string(E_USER_NOTICE));
  tlib_pass_if_str_equal("Known error type", "E_USER_DEPRECATED",
                         nr_get_error_type_string(E_USER_DEPRECATED));
  tlib_pass_if_str_equal("Known error type", "E_DEPRECATED",
                         nr_get_error_type_string(E_DEPRECATED));
  tlib_pass_if_str_equal("Known error type", "E_USER_WARNING",
                         nr_get_error_type_string(E_USER_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_WARNING",
                         nr_get_error_type_string(E_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_CORE_WARNING",
                         nr_get_error_type_string(E_CORE_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_COMPILE_WARNING",
                         nr_get_error_type_string(E_COMPILE_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_RECOVERABLE_ERROR",
                         nr_get_error_type_string(E_RECOVERABLE_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_ERROR",
                         nr_get_error_type_string(E_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_USER_ERROR",
                         nr_get_error_type_string(E_USER_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_CORE_ERROR",
                         nr_get_error_type_string(E_CORE_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_COMPILE_ERROR",
                         nr_get_error_type_string(E_COMPILE_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_PARSE",
                         nr_get_error_type_string(E_PARSE));
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);

  test_error_get_priority();
  test_get_error_type_string();

  tlib_php_engine_destroy(TSRMLS_C);
}
