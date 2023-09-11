/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
static void test_op_array_wraprec(TSRMLS_D) {
  zend_op_array oparray = {.function_name = (void*)1};
  nruserfn_t func = {0};

  tlib_php_request_start();

  nr_php_op_array_set_wraprec(&oparray, &func TSRMLS_CC);
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC),
                         &func);

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  /*
   * Invalidate the cached pid.
   */
  NRPRG(pid) -= 1;

  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC), NULL);

  /*
   * Restore the cached pid and invalidate the mangled pid/index value.
   */
  NRPRG(pid) += 1;

  {
    unsigned long pval;

    pval = (unsigned long)oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)];
    oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = (void*)(pval * 2);
  }

  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC), NULL);
#endif /* PHP >= 7.3 */

  tlib_php_request_end();
}
#endif /* PHP < 7.4 */

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
static void test_hashmap_wraprec() {
  const char* user_func1_name = "user_function_to_be_instrumented";
  zend_function* user_func1_zf;
  const char* user_func2_name = "user_callable_to_be_instrumented";
  zend_function* user_func2_zf;
  const char* user_func3_name = "user_function_not_instrumented";
  zend_function* user_func3_zf;
  zend_function* zf;
  char* php_code;
  nruserfn_t *user_func1_wraprec, *user_func2_wraprec, *wraprec_found;

  tlib_php_request_start();

  /* create zend_functions for test user function */
  php_code = nr_formatf("function %s() { return 1; }", user_func1_name);
  tlib_php_request_eval(php_code);
  nr_free(php_code);
  php_code = nr_formatf("function %s() { return 2; }", user_func2_name);
  tlib_php_request_eval(php_code);
  nr_free(php_code);
  php_code = nr_formatf("function %s() { return 3; }", user_func3_name);
  tlib_php_request_eval(php_code);
  nr_free(php_code);
  user_func1_zf = nr_php_find_function(user_func1_name);
  user_func2_zf = nr_php_find_function(user_func2_name);
  user_func3_zf = nr_php_find_function(user_func3_name);

  /* assert we got two distinct zend_functions */
  tlib_fail_if_ptr_equal("zend_functions are different", user_func1_zf,
                         user_func2_zf);
  tlib_fail_if_ptr_equal("zend_functions are different", user_func1_zf,
                         user_func3_zf);
  tlib_fail_if_ptr_equal("zend_functions are different", user_func2_zf,
                         user_func3_zf);

  /* don't create wraprec yet */
  user_func1_wraprec = NULL;
  wraprec_found = nr_php_get_wraprec(user_func1_zf);
  tlib_pass_if_ptr_equal("lookup uninstrumented user function", wraprec_found,
                         user_func1_wraprec);

  /* instrument user function */
  nr_wrap_user_function_options_t options = {
    NR_WRAPREC_NOT_TRANSIENT,
    NR_WRAPREC_CREATE_INSTRUMENTED_FUNCTION_METRIC
  };
  user_func1_wraprec = nr_php_add_custom_tracer_named(
      user_func1_name, nr_strlen(user_func1_name), options );
  wraprec_found = nr_php_get_wraprec(user_func1_zf);
  tlib_pass_if_ptr_equal("lookup instrumented user function succeeds",
                         wraprec_found, user_func1_wraprec);

  /* lookup instrumentation via copy of zend_function pointer */
  zf = user_func1_zf;
  wraprec_found = nr_php_get_wraprec(zf);
  tlib_pass_if_ptr_equal(
      "lookup instrumented user function via different pointer to the same "
      "zend_function succeeds",
      wraprec_found, user_func1_wraprec);

  wraprec_found = nr_php_get_wraprec(user_func3_zf);
  tlib_pass_if_null("lookup uninstrumented user function fails", wraprec_found);

  /* lookup instrumentation after modifying op_array.reserved */
  user_func1_zf->op_array.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)]
      = (void*)1;
  wraprec_found = nr_php_get_wraprec(user_func1_zf);
  tlib_pass_if_ptr_equal(
      "lookup instrumented user function with modified reserved field succeeds",
      wraprec_found, user_func1_wraprec);

  /* instrument user callable */
  user_func2_wraprec = nr_php_add_custom_tracer_callable(user_func2_zf);
  wraprec_found = nr_php_get_wraprec(user_func1_zf);
  tlib_pass_if_ptr_equal("lookup instrumented user function still succeeds",
                         wraprec_found, user_func1_wraprec);
  wraprec_found = nr_php_get_wraprec(user_func2_zf);
  tlib_pass_if_ptr_equal("lookup instrumented user callable succeeds",
                         wraprec_found, user_func2_wraprec);

  wraprec_found = nr_php_get_wraprec(user_func3_zf);
  tlib_pass_if_null("lookup uninstrumented user function fails", wraprec_found);

  /* lookup instrumentation after modifying op_array.reserved */
  user_func1_zf->op_array.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)]
      = (void*)1;
  wraprec_found = nr_php_get_wraprec(user_func2_zf);
  tlib_pass_if_ptr_equal(
      "lookup instrumented user callable with modified reserved field succeeds",
      wraprec_found, user_func2_wraprec);

  tlib_php_request_end();
}
#endif /* PHP >= 7.4 */

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  test_op_array_wraprec(TSRMLS_C);
#else
  test_hashmap_wraprec();
#endif /* PHP >= 7.4 */

  tlib_php_engine_destroy(TSRMLS_C);
}
