/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_wrapper.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
NR_PHP_WRAPPER(test_add_array) {
  zval* arg = nr_php_zval_alloc();

  (void)wraprec;

  array_init(arg);
  nr_php_arg_add(NR_EXECUTE_ORIG_ARGS, arg);
  nr_php_zval_free(&arg);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(test_add_2_arrays) {
  zval* arg = NULL;

  (void)wraprec;

  arg = nr_php_zval_alloc();
  array_init(arg);
  nr_php_arg_add(NR_EXECUTE_ORIG_ARGS, arg);
  nr_php_zval_free(&arg);

  arg = nr_php_zval_alloc();
  array_init(arg);
  nr_php_arg_add(NR_EXECUTE_ORIG_ARGS, arg);
  nr_php_zval_free(&arg);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END
static void test_add_arg(TSRMLS_D) {
  zval* expr = NULL;
  zval* arg = NULL;

  tlib_php_request_start();

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  tlib_php_request_eval("function arg0_def0() { return 4; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(NR_PSTR("arg0_def0"), test_add_array,
                                         NULL TSRMLS_CC);

  tlib_php_request_eval("function arg1_def0($a) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(NR_PSTR("arg1_def0"), test_add_array,
                                         NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg0_def1($a = null) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(NR_PSTR("arg0_def1"), test_add_array,
                                         NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(NR_PSTR("arg1_def1"), test_add_array,
                                         NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1_2($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(NR_PSTR("arg1_def1_2"),
                                         test_add_2_arrays, NULL TSRMLS_CC);

  tlib_php_request_eval("function splat(...$a) { return $a[0]; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(NR_PSTR("splat"), test_add_array,
                                         NULL TSRMLS_CC);
#else
  tlib_php_request_eval("function arg0_def0() { return 4; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("arg0_def0"), test_add_array TSRMLS_CC);

  tlib_php_request_eval("function arg1_def0($a) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("arg1_def0"), test_add_array TSRMLS_CC);

  tlib_php_request_eval(
      "function arg0_def1($a = null) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("arg0_def1"), test_add_array TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("arg1_def1"), test_add_array TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1_2($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("arg1_def1_2"),
                            test_add_2_arrays TSRMLS_CC);

  tlib_php_request_eval("function splat(...$a) { return $a[0]; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("splat"), test_add_array TSRMLS_CC);
#endif
  /*
   * 0 arguments, 0 default arguments, 0 arguments given
   */
  expr = nr_php_call(NULL, "arg0_def0");
  tlib_pass_if_not_null("0 args, 0 default args, 0 given", expr);
  tlib_pass_if_zval_type_is("0 args, 0 default args, 0 given", IS_LONG, expr);
  nr_php_zval_free(&expr);

  /*
   * 0 arguments, 0 default arguments, 1 argument given
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "arg0_def0", arg);
  tlib_pass_if_not_null("0 args, 0 default args, 1 given", expr);
  tlib_pass_if_zval_type_is("0 args, 0 default args, 1 given", IS_LONG, expr);
  nr_php_zval_free(&arg);
  nr_php_zval_free(&expr);

  /*
   * 1 argument, 0 default arguments, 0 arguments given
   */

  expr = nr_php_call(NULL, "arg1_def0");
  tlib_pass_if_not_null("1 args, 0 default args, 0 given", expr);
  tlib_pass_if_zval_type_is("1 args, 0 default args, 0 given", IS_ARRAY, expr);
  nr_php_zval_free(&expr);

  /*
   * 1 argument, 0 default arguments, 1 argument given
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "arg1_def0", arg);
  tlib_pass_if_not_null("1 args, 0 default args, 1 given", expr);
  tlib_pass_if_zval_type_is("1 args, 0 default args, 1 given", IS_STRING, expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * 0 arguments, 1 default arguments, 0 arguments given
   */
  expr = nr_php_call(NULL, "arg0_def1");
  tlib_pass_if_not_null("0 args, 1 default args, 0 given", expr);
  tlib_pass_if_zval_type_is("0 args, 1 default args, 0 given", IS_ARRAY, expr);
  nr_php_zval_free(&expr);

  /*
   * 0 arguments, 1 default arguments, 1 argument given
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "arg0_def1", arg);
  tlib_pass_if_not_null("0 args, 1 default args, 1 given", expr);
  tlib_pass_if_zval_type_is("0 args, 1 default args, 1 given", IS_STRING, expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * 1 argument, 1 default arguments, 0 arguments given
   */
  expr = nr_php_call(NULL, "arg1_def1");
  tlib_pass_if_not_null("1 args, 1 default args, 0 given", expr);
  tlib_pass_if_zval_type_is("1 args, 1 default args, 0 given", IS_NULL, expr);
  nr_php_zval_free(&expr);

  /*
   * 1 argument, 1 default arguments, 2 arguments given
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "arg1_def1", arg);
  tlib_pass_if_not_null("1 args, 1 default args, 1 given", expr);
  tlib_pass_if_zval_type_is("1 args, 1 default args, 1 given", IS_ARRAY, expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * 1 argument, 1 default arguments, 0 arguments given, 2 added
   */
  expr = nr_php_call(NULL, "arg1_def1_2");
  tlib_pass_if_not_null("1 args, 1 default args, 0 given, 2 added", expr);
  tlib_pass_if_zval_type_is("1 args, 1 default args, 0 given, 2 added",
                            IS_ARRAY, expr);
  nr_php_zval_free(&expr);

  /*
   * 1 argument, 1 default arguments, 1 argument given, 2 added
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "arg1_def1_2", arg);
  tlib_pass_if_not_null("1 args, 1 default args, 1 given, 2 added", expr);
  tlib_pass_if_zval_type_is("1 args, 1 default args, 1 given, 2 added",
                            IS_ARRAY, expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  /*
   * 1 argument, 1 default arguments, 2 arguments given, 2 added
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "arg1_def1_2", arg, arg);
  tlib_pass_if_not_null("1 args, 1 default args, 2 given, 2 added", expr);
  tlib_pass_if_zval_type_is("1 args, 1 default args, 2 given, 2 added",
                            IS_STRING, expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * splat, 0 arguments given
   *
   * nr_php_add_arg does not alter any splat argument lists.
   */
  expr = nr_php_call(NULL, "splat");
  tlib_pass_if_not_null("splat, 0 given", expr);
  tlib_pass_if_zval_type_is("splat, 0 given", IS_NULL, expr);
  nr_php_zval_free(&expr);

  /*
   * splat, 1 argument given,
   *
   * nr_php_add_arg does not alter any splat argument lists.
   */
  arg = tlib_php_request_eval_expr("'a'" TSRMLS_CC);
  expr = nr_php_call(NULL, "splat", arg);
  tlib_pass_if_not_null("splat, 1 given", expr);
  tlib_pass_if_zval_type_is("splat, 1 given", IS_STRING, expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_add_arg();
  tlib_php_engine_destroy(TSRMLS_C);
}
#else  /* PHP 7.3 */
void test_main(void* p NRUNUSED) {}
#endif /* PHP 7.3 */
