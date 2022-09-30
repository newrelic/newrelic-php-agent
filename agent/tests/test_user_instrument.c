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

static void test_get_wraprec_by_name() {
#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
  return;
#else
  zend_op_array oparray = {.function_name = (void*)1};
  zend_function zend_func = {0};
  zend_string* scope_name = NULL;
  zend_class_entry ce = {0};
  nruserfn_t* wraprec = NULL;
  char* name_str = "my_func_name";

  tlib_php_request_start();

  /*
   * NULL if there's no wraprecs in the internal list.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), NULL);

  wraprec = nr_php_add_custom_tracer_named(
      "ClassNoMatch::functionNoMatch",
      nr_strlen("ClassNoMatch::functionNoMatch"));
  wraprec = nr_php_add_custom_tracer_named("functionNoMatch2",
                                           nr_strlen("functionNoMatch2"));
  wraprec = nr_php_add_custom_tracer_named(name_str, nr_strlen(name_str));

  /*
   * NULL if zend_function is NULL.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(NULL), NULL);

  /*
   * NULL if function name is NULL.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), NULL);

  /*
   * NULL if name is correct but type is wrong.
   */
  zend_func.common.function_name
      = zend_string_init(name_str, strlen(name_str), 0);
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), NULL);
  /*
   * Valid if function name matches and type is user function.
   */
  zend_func.type = ZEND_USER_FUNCTION;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), wraprec);
  /*
   * NULL if function name matches, but class name doesn't.
   */
  scope_name = zend_string_init("ClassName", strlen("ClassName"), 0);
  ce.name = scope_name;
  zend_func.common.scope = &ce;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), NULL);
  /*
   * NULL if function name doesn't match and class name matches.
   */

  wraprec = nr_php_add_custom_tracer_named(
      "ClassName::my_func_name2", nr_strlen("ClassName::my_func_name2"));
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), NULL);
  /*
   * Valid if function name matches and class name matches.
   */
  wraprec = nr_php_add_custom_tracer_named(
      "ClassName::my_func_name", nr_strlen("ClassName::my_func_name"));
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), wraprec);

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  /*
   * Invalidate the cached pid.
   */
  NRPRG(pid) -= 1;

  /*
   * Not NULL because we don't care about the reserved array anymore.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), wraprec);
  /*
   * Restore the cached pid and invalidate the mangled pid/index value.
   */
  NRPRG(pid) += 1;

  {
    unsigned long pval;

    pval = (unsigned long)oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)];
    oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = (void*)(pval * 2);
  }
  /*
   * Not NULL because we don't care about the reserved array anymore.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(&zend_func), wraprec);

#endif /* PHP >= 7.3 */
  zend_string_release(zend_func.common.function_name);
  zend_string_release(zend_func.common.scope->name);
  tlib_php_request_end();
#endif
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);
  test_op_array_wraprec(TSRMLS_C);
  test_get_wraprec_by_name();

  tlib_php_engine_destroy(TSRMLS_C);
}
