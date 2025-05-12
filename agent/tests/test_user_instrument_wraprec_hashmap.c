/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument_wraprec_hashmap.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

// clang-format off

#define SCOPE_NAME "Vendor\\Namespace\\ClassName"
#define METHOD_NAME "getSomething"
#define SCOPED_METHOD_NAME SCOPE_NAME "::" METHOD_NAME
#define FUNCTION_NAME "global_function"

static void test_wraprecs_hashmap() {
  nruserfn_t *wraprec, *another_method_wraprec, *yet_another_method_wraprec, *found_wraprec;
  zend_string *func_name, *scope_name, *method_name, *scope, *method;
  bool is_new_wraprec = false;
  bool *is_new_wraprec_tests[] = {NULL, &is_new_wraprec};

  func_name = zend_string_init(NR_PSTR(FUNCTION_NAME), 0);
  scope_name = zend_string_init(NR_PSTR(SCOPE_NAME), 0);
  method_name = zend_string_init(NR_PSTR(METHOD_NAME), 0);

  for (size_t i = 0; i < sizeof(is_new_wraprec_tests) / sizeof(is_new_wraprec_tests[0]); i++) {
    bool *is_new_wraprec_ptr = is_new_wraprec_tests[i];
    // user_instrument_wraprec_hashmap is initialized at minit
    // destroy it to test agent's behavior when it is not initialized
    nr_php_user_instrument_wraprec_hashmap_destroy();

    // Test valid operations before initializing the hashmap
    wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(FUNCTION_NAME),
                                                        is_new_wraprec_ptr);
    tlib_pass_if_null("adding valid function before init", wraprec);
    if (NULL != is_new_wraprec_ptr) {
    tlib_pass_if_false("adding valid function before init", *is_new_wraprec_ptr,
                      "expected false for is_new_wraprec");
    }
    wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPED_METHOD_NAME),
                                                        is_new_wraprec_ptr);
    tlib_pass_if_null("adding valid method before init", wraprec);
    if (NULL != is_new_wraprec_ptr) {
    tlib_pass_if_false("adding valid function before init", *is_new_wraprec_ptr,
                      "expected false for is_new_wraprec");
    }

    // Initialize the hashmap
    nr_php_user_instrument_wraprec_hashmap_init();

    // Test valid operations after initializing the hashmap
    wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(FUNCTION_NAME),
                                                        is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding valid global function", wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_true("adding valid global function", *is_new_wraprec_ptr,
                        "expected true for is_new_wraprec");
    }

    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(func_name, NULL);
    tlib_pass_if_ptr_equal("getting valid global function", wraprec, found_wraprec);

    wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(FUNCTION_NAME),
                                                        is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding valid global function one more time", wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_false("adding valid global function one more time", *is_new_wraprec_ptr,
                        "expected false for is_new_wraprec");
    }
    tlib_pass_if_ptr_equal("getting valid global function one more time", wraprec, found_wraprec);

    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(func_name, scope_name);
    tlib_pass_if_null("getting global function with scope", found_wraprec);

    wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPED_METHOD_NAME),
                                                        is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding valid scoped method", wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_true("adding valid scoped function", *is_new_wraprec_ptr,
                        "expected true for is_new_wraprec");
    }

    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
    tlib_pass_if_ptr_equal("getting scoped method", wraprec, found_wraprec);

    wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPED_METHOD_NAME),
                                                        is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding valid scoped method one more time", wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_false("adding valid scoped method one more time", *is_new_wraprec_ptr,
                        "expected false for is_new_wraprec");
    }
    tlib_pass_if_ptr_equal("getting valid scoped method one more time", wraprec, found_wraprec);

    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(NULL, scope_name);
    tlib_pass_if_null("getting scoped method without method name", found_wraprec);

    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(method_name, NULL);
    tlib_pass_if_null("getting scoped method without scope", found_wraprec);

    another_method_wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPE_NAME "::anotherMethod"),
                                                                        is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding another scoped method", another_method_wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_true("adding another scoped method", *is_new_wraprec_ptr,
                        "expected true for is_new_wraprec");
    }
    scope = zend_string_init_fast(NR_PSTR(SCOPE_NAME));
    method = zend_string_init_fast(NR_PSTR("anotherMethod"));
    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(method, scope);
    tlib_pass_if_ptr_equal("getting another scoped method", another_method_wraprec, found_wraprec);

    another_method_wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPE_NAME "::anotherMethod"),
                                                                        is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding another scoped method one more time", another_method_wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_false("adding another scoped method one more time", *is_new_wraprec_ptr,
                         "expected false for is_new_wraprec");
    }
    tlib_pass_if_ptr_equal("getting another scoped method one more time", another_method_wraprec, found_wraprec);
    zend_string_free(method);
    zend_string_free(scope);

    yet_another_method_wraprec = nr_php_user_instrument_wraprec_hashmap_add(NR_PSTR(SCOPE_NAME "::anotherMethodStill"),
                                                                            is_new_wraprec_ptr);
    tlib_pass_if_not_null("adding yet another scoped method", yet_another_method_wraprec);
    if (NULL != is_new_wraprec_ptr) {
      tlib_pass_if_true("adding yet another scoped method", *is_new_wraprec_ptr,
                        "expected true for is_new_wraprec");
    }
    scope = zend_string_init_fast(NR_PSTR(SCOPE_NAME));
    method = zend_string_init_fast(NR_PSTR("anotherMethodStill"));
    found_wraprec = nr_php_user_instrument_wraprec_hashmap_get(method, scope);
    tlib_pass_if_ptr_equal("getting another scoped method", yet_another_method_wraprec, found_wraprec);
    zend_string_free(method);
    zend_string_free(scope);

    nr_php_user_instrument_wraprec_hashmap_destroy();
  }

  zend_string_free(func_name);
  zend_string_free(scope_name);
  zend_string_free(method_name);
}

// clang-format on

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_wraprecs_hashmap();

  tlib_php_engine_destroy(TSRMLS_C);
}
