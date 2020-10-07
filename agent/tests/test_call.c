/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_callable(TSRMLS_D) {
  zval* callable;
  zval* param;
  zval* retval;

  tlib_php_request_start();

  /*
   * Test : Invalid parameters (basically: does it crash?).
   */
  tlib_pass_if_null("NULL callable", nr_php_call_callable(NULL));

  /*
   * Test : No parameters.
   */
  tlib_php_request_eval("function life() { return 42; }" TSRMLS_CC);
  tlib_php_request_eval(
      "class C { static function life() { return 42; } function universe() { "
      "return 42; } }" TSRMLS_CC);

  callable = tlib_php_request_eval_expr("'life'" TSRMLS_CC);
  retval = nr_php_call_callable(callable);
  tlib_pass_if_not_null("simple callable, no params", retval);
  tlib_pass_if_zval_type_is("simple callable, no params", IS_LONG, retval);
  tlib_pass_if_long_equal("simple callable, no params", 42,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable = tlib_php_request_eval_expr("array(new C, 'universe')" TSRMLS_CC);
  retval = nr_php_call_callable(callable);
  tlib_pass_if_not_null("method callable, no params", retval);
  tlib_pass_if_zval_type_is("method callable, no params", IS_LONG, retval);
  tlib_pass_if_long_equal("method callable, no params", 42,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable = tlib_php_request_eval_expr("array('C', 'life')" TSRMLS_CC);
  retval = nr_php_call_callable(callable);
  tlib_pass_if_not_null("static method callable, no params", retval);
  tlib_pass_if_zval_type_is("static method callable, no params", IS_LONG,
                            retval);
  tlib_pass_if_long_equal("static method callable, no params", 42,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable = tlib_php_request_eval_expr("'C::life'" TSRMLS_CC);
  retval = nr_php_call_callable(callable);
  tlib_pass_if_not_null("string static method callable, no params", retval);
  tlib_pass_if_zval_type_is("string static method callable, no params", IS_LONG,
                            retval);
  tlib_pass_if_long_equal("string static method callable, no params", 42,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable = tlib_php_request_eval_expr("function () { return 42; }" TSRMLS_CC);
  retval = nr_php_call_callable(callable);
  tlib_pass_if_not_null("closure callable, no params", retval);
  tlib_pass_if_zval_type_is("closure callable, no params", IS_LONG, retval);
  tlib_pass_if_long_equal("closure callable, no params", 42,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

#ifdef PHP7
  callable = tlib_php_request_eval_expr(
      "new class { function __invoke() { return 42; } }" TSRMLS_CC);
  retval = nr_php_call_callable(callable);
  tlib_pass_if_not_null("anonymous class callable, no params", retval);
  tlib_pass_if_zval_type_is("anonymous class callable, no params", IS_LONG,
                            retval);
  tlib_pass_if_long_equal("anonymous class callable, no params", 42,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);
#endif /* PHP7 */

  /*
   * Test : With parameters.
   */
  tlib_php_request_eval("function square($n) { return $n * $n; }" TSRMLS_CC);
  tlib_php_request_eval(
      "class Squarer { static function statSquare($n) { return square($n); } "
      "function square($n) { return square($n); } }" TSRMLS_CC);
  param = tlib_php_request_eval_expr("2" TSRMLS_CC);

  callable = tlib_php_request_eval_expr("'square'" TSRMLS_CC);
  retval = nr_php_call_callable(callable, param);
  tlib_pass_if_not_null("simple callable, one param", retval);
  tlib_pass_if_zval_type_is("simple callable, one param", IS_LONG, retval);
  tlib_pass_if_long_equal("simple callable, one param", 4,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable
      = tlib_php_request_eval_expr("array(new Squarer, 'square')" TSRMLS_CC);
  retval = nr_php_call_callable(callable, param);
  tlib_pass_if_not_null("method callable, one param", retval);
  tlib_pass_if_zval_type_is("method callable, one param", IS_LONG, retval);
  tlib_pass_if_long_equal("method callable, one param", 4,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable
      = tlib_php_request_eval_expr("array('Squarer', 'statSquare')" TSRMLS_CC);
  retval = nr_php_call_callable(callable, param);
  tlib_pass_if_not_null("static method callable, one param", retval);
  tlib_pass_if_zval_type_is("static method callable, one param", IS_LONG,
                            retval);
  tlib_pass_if_long_equal("static method callable, one param", 4,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable = tlib_php_request_eval_expr("'Squarer::statSquare'" TSRMLS_CC);
  retval = nr_php_call_callable(callable, param);
  tlib_pass_if_not_null("string static method callable, one param", retval);
  tlib_pass_if_zval_type_is("string static method callable, one param", IS_LONG,
                            retval);
  tlib_pass_if_long_equal("string static method callable, one param", 4,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

  callable = tlib_php_request_eval_expr(
      "function ($n) { return square($n); }" TSRMLS_CC);
  retval = nr_php_call_callable(callable, param);
  tlib_pass_if_not_null("closure callable, one param", retval);
  tlib_pass_if_zval_type_is("closure callable, one param", IS_LONG, retval);
  tlib_pass_if_long_equal("closure callable, one param", 4,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);

#ifdef PHP7
  callable = tlib_php_request_eval_expr(
      "new class { function __invoke($n) { return square($n); } }" TSRMLS_CC);
  retval = nr_php_call_callable(callable, param);
  tlib_pass_if_not_null("anonymous class callable, one param", retval);
  tlib_pass_if_zval_type_is("anonymous class callable, one param", IS_LONG,
                            retval);
  tlib_pass_if_long_equal("anonymous class callable, one param", 4,
                          (long)Z_LVAL_P(retval));
  nr_php_zval_free(&callable);
  nr_php_zval_free(&retval);
#endif /* PHP7 */

  nr_php_zval_free(&param);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_callable(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
