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

/*
 * Set test_before, test_after, test_clean to use a Newrelic global variable.
 * Randomly picking NRPRG(drupal_http_request_depth) because it is easy to use,
 * a variable, and not used in any other way in this test.
 *
 */

NR_PHP_WRAPPER(test_before) {
  (void)wraprec;
  NRPRG(drupal_http_request_depth) = 10;

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(test_after) {
  (void)wraprec;
  NRPRG(drupal_http_request_depth) = 20;

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(test_clean) {
  (void)wraprec;
  if (20 != NRPRG(drupal_http_request_depth)) {
    NRPRG(drupal_http_request_depth) = 30;
  }
  /*
   * If 20 = NRPRG(drupal_http_request_depth) it means the after callback was
   * called. We should never call clean if the after callback was called.
   */

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

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
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("arg0_def0"), test_add_array, NULL, NULL TSRMLS_CC);

  tlib_php_request_eval("function arg1_def0($a) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("arg1_def0"), test_add_array, NULL, NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg0_def1($a = null) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("arg0_def1"), test_add_array, NULL, NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("arg1_def1"), test_add_array, NULL, NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1_2($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("arg1_def1_2"), test_add_2_arrays, NULL, NULL TSRMLS_CC);

  tlib_php_request_eval("function splat(...$a) { return $a[0]; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("splat"), test_add_array,
                                               NULL, NULL TSRMLS_CC);
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

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
static void test_before_after_clean() {
  zval* expr = NULL;
  zval* arg = NULL;

  /*
   * before, after, clean callbacks are all set.
   */
  tlib_php_request_start();

  tlib_php_request_eval(
      "function all_set($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("all_set"), test_before,
                                               test_after, test_clean);
  /*
   * pass argument that will not throw exception.
   * before/after should be called.
   * clean callback should not be called.
   */
  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "all_set", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("After callback should set value", 20,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * before should be called after should not be called.
   * clean callback should be called.
   */

  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "all_set", arg);

  tlib_pass_if_null("Exception so expr should be null.", expr);
  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal("Clean callback should set value", 30,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();

  /*
   * before, after, callbacks are set
   */

  tlib_php_request_start();
  tlib_php_request_eval(
      "function before_after($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("before_after"),
                                               test_before, test_after, NULL);

  /*
   * pass argument that will not throw exception.
   * before/after should be called.
   * no clean callback
   */

  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "before_after", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("After callback should set value", 20,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * before should be called after should not be called.
   * clean should not be called.
   */
  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "before_after", arg);
  tlib_pass_if_null("Exception so does not evaluate.", expr);
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal("Clean callback should not set value", 10,
                         NRPRG(drupal_http_request_depth));
  tlib_pass_if_int_equal(
      "Since there is no clean and after doesn't get called, only the before "
      "value persists and does not get cleaned up.",
      10, NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  tlib_php_request_end();

  /*
   * before, clean callbacks are set
   */

  tlib_php_request_start();

  tlib_php_request_eval(
      "function before_clean($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("before_clean"),
                                               test_before, NULL, test_clean);

  /*
   * pass argument that will not throw exception.
   * before should be called.
   * clean callback should not be called.
   */

  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "before_clean", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("Only before callback should set value", 10,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * no before callback.
   * clean callback should be called.
   */
  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "before_clean", arg);
  tlib_pass_if_null("Exception so func does not evaluate.", expr);
  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal("Clean callback should set value", 30,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();

  /*
   * after, clean callbacks are  set
   */
  tlib_php_request_start();

  tlib_php_request_eval(
      "function after_clean($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("after_clean"), NULL,
                                               test_after, test_clean);
  /*
   * pass argument that will not throw exception.
   * after should be called.
   * clean callback should not be called.
   */
  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "after_clean", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("After callback should set value", 20,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * before should be called after should not be called.
   * clean callback should be called.
   */
  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "after_clean", arg);
  tlib_pass_if_null("Exception so returns null.", expr);
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal(
      "After callback should not be called and clean callback should set value",
      30, NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  tlib_php_request_end();

  /*
   * before only callback
   */
  tlib_php_request_start();

  tlib_php_request_eval(
      "function before_only($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("before_only"),
                                               test_before, NULL, NULL);
  /*
   * pass argument that will not throw exception.
   * before should be called.
   * no other callbacks should be called.
   */
  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "before_only", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("Before callback should set value", 10,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * before should be called after should not be called.
   * clean callback should be called.
   */
  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "before_only", arg);
  tlib_pass_if_null("Exception so does not evaluate.", expr);
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal("Only before would set the value", 10,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  tlib_php_request_end();

  /*
   * after only callback
   */
  tlib_php_request_start();

  tlib_php_request_eval(
      "function after_only($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("after_only"), NULL,
                                               test_after, NULL);
  /*
   * pass argument that will not throw exception.
   * after should be called.
   * no other callbacks should be called.
   */
  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "after_only", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("After callback should set value", 20,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * before should be called after should not be called.
   * clean callback should be called.
   */
  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "after_only", arg);
  tlib_pass_if_null("Exception so should be null.", expr);
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal("No callbacks triggered to set the value", 0,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  tlib_php_request_end();

  /*
   * clean only callback
   */
  tlib_php_request_start();

  tlib_php_request_eval(
      "function clean_only($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after_clean(NR_PSTR("clean_only"), NULL,
                                               NULL, test_clean);
  /*
   * pass argument that will not throw exception.
   * clean should be called.
   * no other callbacks should be called.
   */
  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "clean_only", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal("No callback to set value", 0,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * pass argument that will throw exception.
   * clean callback should be called.
   */
  arg = tlib_php_request_eval_expr("0" TSRMLS_CC);
  expr = nr_php_call(NULL, "clean_only", arg);
  tlib_pass_if_null("Exception so should be null.", expr);
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal("Only clean would set the value", 30,
                         NRPRG(drupal_http_request_depth));
  NRPRG(drupal_http_request_depth) = 0;
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  tlib_php_request_end();
}
#endif

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_add_arg();
  test_before_after_clean();
  tlib_php_engine_destroy(TSRMLS_C);
}
#else  /* PHP 7.3 */
void test_main(void* p NRUNUSED) {}
#endif /* PHP 7.3 */
