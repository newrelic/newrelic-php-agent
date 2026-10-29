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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG_CTX(check_cufa) = true;
#endif

  NRPRG_CTX(check_cufa) = true;
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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG_CTX(check_cufa) = true;
#endif

  NRPRG_CTX(check_cufa) = true;
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

/*
 * Old handlers for nr_zend_call_old_handler_segment: one that does nothing,
 * ones that run PHP code that ends the transaction, or ends it and starts
 * another one, and ones that bail out.
 */
static void old_handler_noop(INTERNAL_FUNCTION_PARAMETERS) {
  (void)execute_data;
  (void)return_value;
}

static void old_handler_ends_txn(INTERNAL_FUNCTION_PARAMETERS) {
  (void)execute_data;
  (void)return_value;
  tlib_php_request_eval("newrelic_end_transaction();" TSRMLS_CC);
}

static void old_handler_restarts_txn(INTERNAL_FUNCTION_PARAMETERS) {
  (void)execute_data;
  (void)return_value;
  tlib_php_request_eval(
      "newrelic_end_transaction();"
      "newrelic_start_transaction(ini_get('newrelic.appname'));" TSRMLS_CC);
}

static void old_handler_bails_out(INTERNAL_FUNCTION_PARAMETERS) {
  (void)execute_data;
  (void)return_value;
  zend_bailout();
}

static void old_handler_ends_txn_and_bails_out(INTERNAL_FUNCTION_PARAMETERS) {
  (void)execute_data;
  (void)return_value;
  tlib_php_request_eval("newrelic_end_transaction();" TSRMLS_CC);
  zend_bailout();
}

static void test_call_old_handler_segment(TSRMLS_D) {
  nr_segment_t* segment;
  int zcaught;

  tlib_php_request_start();

  /*
   * The transaction is the same after the call: the segment is kept.
   */
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  tlib_pass_if_not_null("segment started", segment);
  zcaught = nr_zend_call_old_handler_segment(&segment, old_handler_noop, NULL,
                                             NULL);
  tlib_pass_if_int_equal("no bailout", 0, zcaught);
  tlib_pass_if_not_null("same transaction: segment kept", segment);

  /*
   * The transaction ended during the call: the segment is cleared.
   */
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  nr_zend_call_old_handler_segment(&segment, old_handler_ends_txn, NULL, NULL);
  tlib_pass_if_null("transaction ended: segment cleared", segment);
  tlib_pass_if_null("transaction ended: no transaction", NRPRG(txn));

  tlib_php_request_end();

  /*
   * The transaction ended and another one started during the call: the
   * segment belongs to the transaction that ended and is cleared.
   */
  tlib_php_request_start();
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  nr_zend_call_old_handler_segment(&segment, old_handler_restarts_txn, NULL,
                                   NULL);
  tlib_pass_if_null("transaction restarted: segment cleared", segment);
  tlib_pass_if_not_null("transaction restarted: a new transaction", NRPRG(txn));

  /*
   * No segment: nothing to clear, and a NULL pointer is accepted.
   */
  segment = NULL;
  nr_zend_call_old_handler_segment(&segment, old_handler_restarts_txn, NULL,
                                   NULL);
  tlib_pass_if_null("NULL segment stays NULL", segment);
  zcaught
      = nr_zend_call_old_handler_segment(NULL, old_handler_noop, NULL, NULL);
  tlib_pass_if_int_equal("NULL pointer: no bailout", 0, zcaught);

  tlib_php_request_end();

  /*
   * A bailout is returned and does not change the segment. A bailout leaves
   * the engine with an unclean shutdown, so each case has a request of its own.
   */
  tlib_php_request_start();
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  zcaught = nr_zend_call_old_handler_segment(&segment, old_handler_bails_out,
                                             NULL, NULL);
  tlib_pass_if_int_equal("bailout is returned", 1, zcaught);
  tlib_pass_if_not_null("bailout, same transaction: segment kept", segment);
  tlib_php_request_end();

  /*
   * The same when the old handler bails out after the transaction ended, and
   * the bailout is still returned.
   */
  tlib_php_request_start();
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  zcaught = nr_zend_call_old_handler_segment(
      &segment, old_handler_ends_txn_and_bails_out, NULL, NULL);
  tlib_pass_if_int_equal("transaction ended with a bailout: bailout returned",
                         1, zcaught);
  tlib_pass_if_null("transaction ended with a bailout: segment cleared",
                    segment);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);

  test_cufa_direct(TSRMLS_C);
  test_cufa_indirect(TSRMLS_C);
  test_call_old_handler_segment(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
