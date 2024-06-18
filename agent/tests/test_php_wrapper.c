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
 * endif to match:
 * ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
 *  && !defined OVERWRITE_ZEND_EXECUTE_DATA
 */

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

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
NR_PHP_WRAPPER(test_name_txn_before_not_ok) {
  nr_txn_set_path("UnitTest", NRPRG(txn), wraprec->funcname,
                  NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(test_name_txn_before_ok) {
  nr_txn_set_path("UnitTest", NRPRG(txn), wraprec->funcname,
                  NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(test_name_txn_after_not_ok) {
  NR_PHP_WRAPPER_CALL;
  nr_txn_set_path("UnitTest", NRPRG(txn), wraprec->funcname,
                  NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(test_name_txn_after_ok) {
  NR_PHP_WRAPPER_CALL;
  nr_txn_set_path("UnitTest", NRPRG(txn), wraprec->funcname,
                  NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);
}
NR_PHP_WRAPPER_END

static void populate_functions() {
  tlib_php_request_eval("function three($a) { return $a; }" TSRMLS_CC);
  tlib_php_request_eval("function two($a) { return three($a); }" TSRMLS_CC);
  tlib_php_request_eval("function one($a) { return two($a); }" TSRMLS_CC);
}
/*
 * This function is meant to wrap/test when only ONE before/after special
 * callback is chosen for one, two, and three. If one_before is configured,
 * one_after must be NULL.
 */
static void execute_nested_framework_calls(nrspecialfn_t one_before,
                                           nrspecialfn_t one_after,
                                           nrspecialfn_t two_before,
                                           nrspecialfn_t two_after,
                                           nrspecialfn_t three_before,
                                           nrspecialfn_t three_after,
                                           char* expected_name,
                                           char* message) {
  zval* expr = NULL;
  zval* arg = NULL;

  tlib_php_engine_create("" PTSRMLS_CC);
  tlib_php_request_start();
  populate_functions();

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("one"), one_before, one_after);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("two"), two_before, two_after);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("three"), three_before, three_after);
#else
  /*
   * This will pick up whichever one isn't null.
   */
  nr_php_wrap_user_function(NR_PSTR("one"), one_before TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("two"), two_before TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("three"), three_before TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("one"), one_after TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("two"), two_after TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("three"), three_after TSRMLS_CC);
#endif

  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "one", arg);
  tlib_pass_if_not_null("Runs fine.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_str_equal(message, expected_name, NRTXN(path));

  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();
  tlib_php_engine_destroy(TSRMLS_C);
}

/*
 * to customize txn naming per framework, PHP agent uses the order in which
 * (functions are processed, NR_NOT_OK_TO_OVERWRITE/NR_OK_TO_OVERWRITE, and
 * whether it is called either before or after NR_PHP_WRAPPER_CALL (for pre PHP
 * 8+) or whether it is called in func_begin or func_end (for PHP 8+ / OAPI).
 *
 * This test serves to illustrate and test *framework* txn naming conventions
 * and how they are affected by calling nr_tx_set path either 1) before
 * nr_wrapper_call for preoapi and in func_begin as a before callback in oapi 2)
 * after nr_wrapper_call for preoapi and in func_end as an after callback in
 * oapi
 *
 * The execution order is as follows:
 * 1) oapi: `one` before callback OR legacy:any statements before the `one`
 * PHP_CALL_WRAPPER
 *
 *     2) `one` begins execution and calls `two`
 *
 *         3) oapi: `two` before callback OR legacy: any statements before the
 * `two` PHP_CALL_WRAPPER
 *
 *              4) `two` begins execution and calls `three`
 *
 *                  5) oapi: `three` before callback OR legacy: any statements
 * before the `three` PHP_CALL_WRAPPER
 *
 *                      6) `three` begins execution
 *
 *                      7) `three` ends
 *
 *                   8) oapi: `three` after callback OR legacy: any statements
 * after the `three` PHP_CALL_WRAPPER
 *
 *               9) `two` ends
 *
 *          10) oapi:`two` after callback OR legacy: any statements after the
 * `two` PHP_CALL_WRAPPER
 *
 *      11) `one` ends
 *
 * 12) oapi: `one` after callback OR legacy: any statements after the `one`
 * PHP_CALL_WRAPPER
 */
static void test_framework_txn_naming() {
  /*
   * This function both tests and illustrates how to use the wrapped function
   * special callbacks in the various framework scenarios for naming the txn
   * when there are multiple routes for naming (i.e. nested calls that have
   * wrapper special functions that all call nr_set_txn_path in different ways.
   * Each test case can be considered a "framework" with three different ways of
   * naming the txn called by the three nested functions. For all cases,
   * function `one` calls `two` calls `three`.
   */

  /*
   * case 1) IF wrapper function is called before NR_PHP_WRAPPER_CALL or called
   * in func_begin AND NR_NOT_OK_TO_OVERWRITE is set THEN the FIRST wrapped
   * function encountered determines the txn name.
   *
   * All functions set the txn before NR_PHP_WRAPPER_CALL and/or in OAPI
   * func_begin and use NR_NOT_OK_TO_OVERWRITE.
   *
   * Expecting `one` to name txn.
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, test_name_txn_before_not_ok, NULL,
      test_name_txn_before_not_ok, NULL, "one",
      "one:name_before_call:will_not_overwrite,two:name_before_call:will_not_"
      "overwrite,three:name_before_call:will_not_overwrite");

  /*
   *
   * 2) IF wrapper function is called before NR_PHP_WRAPPER_CALL or called in
   * func_begin AND NR_OK_TO_OVERWRITE is set THEN the LAST wrapped function
   * encountered determines the txn name.
   *
   * All functions set the txn before NR_PHP_WRAPPER_CALL and/or in OAPI
   * func_begin and use NR_OK_TO_OVERWRITE.
   *
   * Expecting `three` to name txn.
   */
  execute_nested_framework_calls(
      test_name_txn_before_ok, NULL, test_name_txn_before_ok, NULL,
      test_name_txn_before_ok, NULL, "three",
      "one:name_before_call:will_overwrite,two:name_before_call:will_overwrite,"
      "three:name_before_call:will_overwrite");
  /*
   *
   * 3) IF wrapper function is called after NR_PHP_WRAPPER_CALL or called in
   * func_end AND NR_NOT_OK_TO_OVERWRITE is set THEN the LAST wrapped function
   * encountered determines the txn name.
   * All functions set the txn after NR_PHP_WRAPPER_CALL and/or in OAPI
   * func_begin and use NR_NOT_OK_TO_OVERWRITE.
   *
   * Expecting `three` to name txn.
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_not_ok, NULL, test_name_txn_after_not_ok, NULL,
      test_name_txn_after_not_ok, "three",
      "one:name_after_call:will_not_overwrite,two:name_after_call:will_not_"
      "overwrite,three:name_after_call:will_not_overwrite");

  /*
   * 4) IF wrapper function is called after NR_PHP_WRAPPER_CALL or called in
   * func_end AND NR_OK_TO_OVERWRITE is set THEN the FIRST wrapped function
   * encountered determines the txn name.
   *
   * All functions set the txn before NR_PHP_WRAPPER_CALL and/or in OAPI
   * func_begin and use NR_OK_TO_OVERWRITE.
   *
   * Expecting `one` to name txn.
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_ok, NULL, test_name_txn_after_ok, NULL,
      test_name_txn_after_ok, "one",
      "one:name_after_call:will_overwrite,two:name_after_call:will_overwrite,"
      "three:name_after_call:will_overwrite");

  /*
   * 5) If there are nested functions that have wrapped functions called before
   * NR_PHP_WRAPPER_CALL or called in func_begin AND that also have called after
   * NR_PHP_WRAPPER_CALL or called in func_end if the after call uses
   * NR_NOT_OK_TO_OVERWRITE, then rule 1 or 2 applies depending on whether a
   * before_func used NR_NOT_OK_TO_OVERWRITE or NR_NOT_TO_OVERWRITE.
   *
   * 6) If there are nested functions that have wrapped functions called before
   * NR_PHP_WRAPPER_CALL or called in func_begin AND that also have called after
   * NR_PHP_WRAPPER_CALL or called in func_end if the after call uses
   * NR_OK_TO_OVERWRITE, then rule 4 applies.
   *
   * Basically a mix and match situation where we have some best tries at naming
   * a nested function transaction via one txn naming wrapped function, but if
   * something better comes along via a DIFFERENT wrapped function, we'd prefer
   * that. Special functions are mixed with regards to calling before/after and
   * NR_NOT_OK_TO_OVERWRITE/NR_OK_TO_OVERWRITE
   */

  /*
   * After only mixes of NR_NOT_OK_TO_OVERWRITE/NR_NOT_OK_TO_OVERWRITE
   */

  /*
   * special function for one is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_not_ok, NULL, test_name_txn_after_ok, NULL,
      test_name_txn_after_ok, "two",
      "one:name_after_call:will_not_overwrite,two:name_after_call:will_"
      "overwrite,three:name_after_call:will_overwrite");

  /*
   * special function for one is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_not_ok, NULL, test_name_txn_after_not_ok, NULL,
      test_name_txn_after_ok, "three",
      "one:name_after_call:will_not_overwrite,two:name_after_call:will_not_"
      "overwrite,three:name_after_call:will_overwrite");

  /*
   * special function for one is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_not_ok, NULL, test_name_txn_after_ok, NULL,
      test_name_txn_after_not_ok, "two",
      "one:name_after_call:will_not_overwrite,two:name_after_call:will_"
      "overwrite,three:name_after_call:will_not_overwrite");

  /*
   * special function for one is called after and uses NR_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `one`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_ok, NULL, test_name_txn_after_not_ok, NULL,
      test_name_txn_after_ok, "one",
      "one:name_after_call:will_overwrite,two:name_after_call:will_not_"
      "overwrite,three:name_after_call:will_overwrite");

  /*
   * special function for one is called after and uses NR_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `one`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_ok, NULL, test_name_txn_after_not_ok, NULL,
      test_name_txn_after_not_ok, "one",
      "one:name_after_call:will_overwrite,two:name_after_call:will_not_"
      "overwrite,three:name_after_call:will_not_overwrite");

  /*
   * special function for one is called after and uses NR_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `one`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_ok, NULL, test_name_txn_after_ok, NULL,
      test_name_txn_after_not_ok, "one",
      "one:name_after_call:will_overwrite,two:name_after_call:will_overwrite,"
      "three:name_after_call:will_not_overwrite");

  /*
   * Before only mixes of NR_NOT_OK_TO_OVERWRITE/NR_NOT_OK_TO_OVERWRITE
   */

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, test_name_txn_before_ok, NULL,
      test_name_txn_before_ok, NULL, "three",
      "one:name_before_call:will_not_overwrite,two:name_before_call:will_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, test_name_txn_before_not_ok, NULL,
      test_name_txn_before_ok, NULL, "three",
      "one:name_before_call:will_not_overwrite,two:name_before_call:will_not_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, test_name_txn_before_ok, NULL,
      test_name_txn_before_not_ok, NULL, "two",
      "one:name_before_call:will_not_overwrite,two:name_before_call:will_"
      "overwrite,three:name_before_call:will_not_overwrite");

  /*
   * special function for one is called before and uses NR_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      test_name_txn_before_ok, NULL, test_name_txn_before_not_ok, NULL,
      test_name_txn_before_ok, NULL, "three",
      "one:name_before_call:will_overwrite,two:name_before_call:will_not_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called before and uses NR_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `one`
   */
  execute_nested_framework_calls(
      test_name_txn_before_ok, NULL, test_name_txn_before_not_ok, NULL,
      test_name_txn_before_not_ok, NULL, "one",
      "one:name_before_call:will_overwrite,two:name_before_call:will_not_"
      "overwrite,three:name_before_call:will_not_overwrite");

  /*
   * special function for one is called before and uses NR_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      test_name_txn_before_ok, NULL, test_name_txn_before_ok, NULL,
      test_name_txn_before_not_ok, NULL, "two",
      "one:name_before_call:will_overwrite,two:name_before_call:will_overwrite,"
      "three:name_before_call:will_not_overwrite");

  /*
   * Before/After and NR_NOT_OK_TO_OVERWRITE/NR_NOT_OK_TO_OVERWRITE mixes
   */

  /*
   * special function for one is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_not_ok, test_name_txn_before_ok, NULL,
      test_name_txn_before_ok, NULL, "three",
      "one:name_after_call:will_not_overwrite,two:name_before_call:will_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called after and uses NR_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `one`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_ok, test_name_txn_before_ok, NULL,
      test_name_txn_before_ok, NULL, "one",
      "one:name_after_call:will_overwrite,two:name_before_call:will_overwrite,"
      "three:name_before_call:will_overwrite");

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, NULL, test_name_txn_after_not_ok,
      test_name_txn_before_ok, NULL, "three",
      "one:name_before_call:will_not_overwrite,two:name_after_call:will_not_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, NULL, test_name_txn_after_ok,
      test_name_txn_before_ok, NULL, "two",
      "one:name_before_call:will_not_overwrite,two:name_after_call:will_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, test_name_txn_before_ok, NULL, NULL,
      test_name_txn_after_not_ok, "two",
      "one:name_before_call:will_not_overwrite,two:name_before_call:will_"
      "overwrite,three:name_after_call:will_not_overwrite");

  /*
   * special function for one is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `three`
   */
  execute_nested_framework_calls(
      test_name_txn_before_not_ok, NULL, test_name_txn_before_ok, NULL, NULL,
      test_name_txn_after_ok, "three",
      "one:name_before_call:will_not_overwrite,two:name_before_call:will_"
      "overwrite,three:name_after_call:will_overwrite");

  /*
   * special function for one is called after and uses NR_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called before and uses NR_OK_TO_OVERWRITE
   *
   * expect txn to be named `one`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_ok, NULL, test_name_txn_after_not_ok,
      test_name_txn_before_ok, NULL, "one",
      "one:name_after_call:will_overwrite,two:name_after_call:will_not_"
      "overwrite,three:name_before_call:will_overwrite");

  /*
   * special function for one is called after and uses NR_OK_TO_OVERWRITE
   * special function for two is called before and uses NR_NOT_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      NULL, test_name_txn_after_not_ok, test_name_txn_before_not_ok, NULL, NULL,
      test_name_txn_after_not_ok, "two",
      "one:name_after_call:will_overwrite,two:name_before_call:will_not_"
      "overwrite,three:name_after_call:will_not_overwrite");

  /*
   * special function for one is called before and uses NR_OK_TO_OVERWRITE
   * special function for two is called after and uses NR_OK_TO_OVERWRITE
   * special function for three is called after and uses NR_NOT_OK_TO_OVERWRITE
   *
   * expect txn to be named `two`
   */
  execute_nested_framework_calls(
      test_name_txn_before_ok, NULL, NULL, test_name_txn_after_ok, NULL,
      test_name_txn_after_not_ok, "two",
      "one:name_before_call:will_overwrite,two:name_after_call:will_overwrite,"
      "three:name_after_call:will_not_overwrite");
}
#endif /* ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO */

static void test_add_arg(TSRMLS_D) {
  zval* expr = NULL;
  zval* arg = NULL;

  tlib_php_request_start();

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  tlib_php_request_eval("function arg0_def0() { return 4; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("arg0_def0"), test_add_array, NULL TSRMLS_CC);

  tlib_php_request_eval("function arg1_def0($a) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("arg1_def0"), test_add_array, NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg0_def1($a = null) { return $a; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("arg0_def1"), test_add_array, NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("arg1_def1"), test_add_array, NULL TSRMLS_CC);

  tlib_php_request_eval(
      "function arg1_def1_2($a, $b = null) { return $b; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("arg1_def1_2"), test_add_2_arrays, NULL TSRMLS_CC);

  tlib_php_request_eval("function splat(...$a) { return $a[0]; }" TSRMLS_CC);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("splat"), test_add_array, NULL TSRMLS_CC);
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
  /*
   * The Jenkins PHP 7.3 nodes are unable to handle the multiple
   * create/destroys, but works on more recent OSs.
   */
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  if (PHP_VERSION_ID < 80000 && PHP_VERSION_ID > 80002) {
    test_framework_txn_naming();
  }
#endif
}
#else  /* PHP 7.3 */
void test_main(void* p NRUNUSED) {}
#endif /* PHP 7.3 */
