/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_wrapper.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

NR_PHP_WRAPPER(test_add_metric_in_wrapper) {
  (void)wraprec;

  nr_segment_add_metric(auto_segment, "metric", true);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

static void test_add_segment_metric(TSRMLS_D) {
  size_t segment_count;
  zval* expr;

  tlib_php_request_start();

  /*
   * Setting this value very high, so segments aren't created on
   * slow machines.
   */
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 1000000;

  tlib_php_request_eval("function f1() { return 4; }" TSRMLS_CC);
  tlib_php_request_eval("function f2() { return 4; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("f2"),
                            test_add_metric_in_wrapper TSRMLS_CC);

  segment_count = NRPRG(txn)->segment_count;

  /*
   * Call a short function with no segment metrics added. This should not
   * increase the segment count.
   */
  expr = nr_php_call(NULL, "f1");
  tlib_pass_if_size_t_equal("no segment created", segment_count,
                            NRPRG(txn)->segment_count);
  nr_php_zval_free(&expr);

  /*
   * Call a short function with segment metrics added. This should increase the
   * segment count.
   */
  expr = nr_php_call(NULL, "f2");
  tlib_pass_if_size_t_equal("segment created", segment_count + 1,
                            NRPRG(txn)->segment_count);
  nr_php_zval_free(&expr);

  tlib_php_request_end();
}

static void test_txn_restart_in_callstack(TSRMLS_D) {
  zval* expr;

  tlib_php_request_start();

  /*
   * Keep all the segments.
   */
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 0;

  tlib_php_request_eval("function f1() { return 4; }" TSRMLS_CC);
  tlib_php_request_eval(
      "function f2() { "
      "newrelic_end_transaction(); "
      "newrelic_start_transaction(\"name\");"
      "}" TSRMLS_CC);
  tlib_php_request_eval("function f3() { f1(); f2(); }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("f1"),
                            test_add_metric_in_wrapper TSRMLS_CC);

  /*
   * This should create a regular segment and a metric for f1, which must get
   * properly cleaned up when the transaction is discarded.
   *
   * This test is primarily for testing that no memory is leaked in this
   * case.
   */
  expr = nr_php_call(NULL, "f3");
  nr_php_zval_free(&expr);

  tlib_php_request_end();
}

static void test_php_cur_stack_depth(TSRMLS_D) {
  zval* expr;

  tlib_php_request_start();

  tlib_php_request_eval("function f1() { return 4; }" TSRMLS_CC);
  tlib_php_request_eval(
      "function f2() { newrelic_ignore_transaction(); return 4; }" TSRMLS_CC);

  expr = nr_php_call(NULL, "f1");
  nr_php_zval_free(&expr);

  tlib_pass_if_int_equal("PHP stack depth tracking when recording", 0,
                         NRPRG(php_cur_stack_depth));

  expr = nr_php_call(NULL, "f2");
  nr_php_zval_free(&expr);

  tlib_pass_if_int_equal("PHP stack depth tracking when ignoring", 0,
                         NRPRG(php_cur_stack_depth));

  tlib_php_request_end();
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */

static void populate_functions() {
  tlib_php_request_eval(
      "function three($a) { if (0 == $a) { throw new "
      "RuntimeException('Division by zero'); } else return $a; }");
  tlib_php_request_eval("function two($a) { return three($a); }");
  tlib_php_request_eval("function uncaught($a) { return two($a); }");
  tlib_php_request_eval(
      "function caught($a) { try {two($a);} catch (Exception $e) { return 1;} "
      "return 1; }");
  tlib_php_request_eval(
      "function followup($a) { try {two($a);} catch (Exception $e) { return "
      "three(1);} return three(1); }");
  tlib_php_request_eval(
      "function followup_uncaught($a) { try {two($a);} catch (Exception $e) { "
      "return three(0);} return three(1); }");
  tlib_php_request_eval(
      "function rethrow($a) { try {two($a);} catch (Exception $e) { throw new "
      "RuntimeException('Rethrown caught exception: '. $e->getMessage());} "
      "return three(1); }");
}

static void test_stack_depth_after_exception() {
  zval* expr = NULL;
  zval* arg = NULL;

  /*
   * stack depth should increment on function call and decrement on function
   * end.
   */
  tlib_php_request_start();
  populate_functions();
  /*
   * pass argument that will not throw exception.
   * stack depth should be 0 before calling and 1 before ending.
   */
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 before function call", 0,
      NRPRG(php_cur_stack_depth));
  arg = tlib_php_request_eval_expr("1" TSRMLS_CC);
  expr = nr_php_call(NULL, "uncaught", arg);
  tlib_pass_if_not_null("Runs fine with no exception.", expr);
  tlib_pass_if_zval_type_is("Should have received the arg value.", IS_LONG,
                            expr);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after successful function call", 0,
      NRPRG(php_cur_stack_depth));

  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);

  /*
   * call a function and trigger an exception
   */
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 before function call", 0,
      NRPRG(php_cur_stack_depth));
  arg = tlib_php_request_eval_expr("0");
  expr = nr_php_call(NULL, "uncaught", arg);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after exception unwind", 0,
      NRPRG(php_cur_stack_depth));
  tlib_pass_if_null("Exception so expr should be null.", expr);

  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after transaction ends", 0,
      NRPRG(php_cur_stack_depth));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();

  /*
   * call a function and trigger an exception that is caught
   */

  tlib_php_request_start();
  populate_functions();

  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 before function call", 0,
      NRPRG(php_cur_stack_depth));
  arg = tlib_php_request_eval_expr("0");
  expr = nr_php_call(NULL, "caught", arg);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after function call", 0,
      NRPRG(php_cur_stack_depth));
  tlib_pass_if_not_null("Exception caught so expr should not be null.", expr);

  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after transaction ends", 0,
      NRPRG(php_cur_stack_depth));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();

  /*
   * call a function and trigger an exception that is caught
   */

  tlib_php_request_start();
  populate_functions();

  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 before function call", 0,
      NRPRG(php_cur_stack_depth));
  arg = tlib_php_request_eval_expr("0");
  expr = nr_php_call(NULL, "followup", arg);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after function call", 0,
      NRPRG(php_cur_stack_depth));
  tlib_pass_if_not_null("Exception caught so expr should not be null.", expr);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after transaction", 0,
      NRPRG(php_cur_stack_depth));

  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after transaction ends", 0,
      NRPRG(php_cur_stack_depth));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();

  /*
   * call a function and trigger an exception that is caught but another
   * uncaught exception is thrown
   */

  tlib_php_request_start();
  populate_functions();

  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 before function call", 0,
      NRPRG(php_cur_stack_depth));
  arg = tlib_php_request_eval_expr("0");
  expr = nr_php_call(NULL, "followup_uncaught", arg);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after exception unwind", 0,
      NRPRG(php_cur_stack_depth));
  tlib_pass_if_null("Exception so expr should not be null.", expr);

  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after transaction ends", 0,
      NRPRG(php_cur_stack_depth));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&arg);
  tlib_php_request_end();

  /*
   * call a function and trigger an exception that is caught then rethrown
   */

  tlib_php_request_start();
  populate_functions();

  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 before function call", 0,
      NRPRG(php_cur_stack_depth));
  arg = tlib_php_request_eval_expr("0");
  expr = nr_php_call(NULL, "rethrow", arg);
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after exception unwind", 0,
      NRPRG(php_cur_stack_depth));
  tlib_pass_if_null("Exception so expr should not be null.", expr);

  /*
   * Trigger the unwind.
   */
  tlib_php_request_eval("newrelic_end_transaction(); ");
  tlib_pass_if_int_equal(
      "PHP stack depth tracking should be 0 after transaction ends", 0,
      NRPRG(php_cur_stack_depth));
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
  test_add_segment_metric(TSRMLS_C);
  test_txn_restart_in_callstack(TSRMLS_C);
  test_php_cur_stack_depth(TSRMLS_C);

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  test_stack_depth_after_exception();
#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
