/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_internal_instrument.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

typedef struct {
  nrphpcufafn_t assert_callback;
  uint64_t call_count;
  const char* message_prefix;
} cufa_pre_state_t;

cufa_pre_state_t cufa_pre_state;

static void cufa_pre(zend_function* func,
                     const zend_function* caller TSRMLS_DC) {
  cufa_pre_state.call_count += 1;
  (cufa_pre_state.assert_callback)(func, caller TSRMLS_CC);
}

static void define_cufa_function_f(TSRMLS_D) {
  tlib_php_request_eval("function f() { return 42; }" TSRMLS_CC);
}

static void cufa_assert_f_called_by_g(zend_function* func,
                                      const zend_function* caller TSRMLS_DC) {
  char* message;

  NR_UNUSED_TSRMLS;

  message = nr_formatf("%s function name", cufa_pre_state.message_prefix);
  tlib_pass_if_str_equal(message, "f", nr_php_function_name(func));
  nr_free(message);

  message = nr_formatf("%s caller name", cufa_pre_state.message_prefix);
  tlib_pass_if_str_equal(message, "g", nr_php_function_name(caller));
  nr_free(message);
}

static void test_cufa_direct(TSRMLS_D) {
  zval* retval = NULL;

  tlib_php_request_start();

  NRPRG(check_cufa) = true;
  define_cufa_function_f(TSRMLS_C);
  tlib_php_request_eval(
      "function g() { return call_user_func_array('f', array()); }" TSRMLS_CC);

  cufa_pre_state = ((cufa_pre_state_t){
      .assert_callback = cufa_assert_f_called_by_g,
      .call_count = 0,
      .message_prefix = "direct",
  });
  nr_php_add_call_user_func_array_pre_callback(cufa_pre TSRMLS_CC);

  retval = tlib_php_request_eval_expr("g()" TSRMLS_CC);
  tlib_pass_if_uint64_t_equal("cufa call count", 1, cufa_pre_state.call_count);
  tlib_pass_if_zval_type_is("cufa return is an integer", IS_LONG, retval);
  tlib_pass_if_int_equal("cufa return value", 42, (int)Z_LVAL_P(retval));
  nr_php_zval_free(&retval);

  tlib_php_request_end();
}

static void test_cufa_indirect(TSRMLS_D) {
  zval* retval = NULL;

  tlib_php_request_start();

  NRPRG(check_cufa) = true;
  define_cufa_function_f(TSRMLS_C);
  tlib_php_request_eval(
      "function g() { $cufa = 'call_user_func_array'; return $cufa('f', "
      "array()); }" TSRMLS_CC);

  cufa_pre_state = ((cufa_pre_state_t){
      .assert_callback = cufa_assert_f_called_by_g,
      .call_count = 0,
      .message_prefix = "indirect",
  });
  nr_php_add_call_user_func_array_pre_callback(cufa_pre TSRMLS_CC);

  retval = tlib_php_request_eval_expr("g()" TSRMLS_CC);
  tlib_pass_if_uint64_t_equal("cufa call count", 1, cufa_pre_state.call_count);
  tlib_pass_if_zval_type_is("cufa return is an integer", IS_LONG, retval);
  tlib_pass_if_int_equal("cufa return value", 42, (int)Z_LVAL_P(retval));
  nr_php_zval_free(&retval);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_cufa_direct(TSRMLS_C);
  test_cufa_indirect(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
