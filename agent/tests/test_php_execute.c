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

#if ZEND_MODULE_API_NO < ZEND_7_0_X_API_NO /* PHP7+ */
  /*
   * Call a short function with no segment metrics added. This should not
   * increase the segment count.
   *
   * With the addition of CLM, we now automatically initialize an empty agent
   * on segment even if it is eventually ends up being empty (i.e., CLM is
   * toggled off).  Therefore there won't be a short segment that doesn't get
   * counted.
   */
  expr = nr_php_call(NULL, "f1");
  tlib_pass_if_size_t_equal("no segment created", segment_count,
                            NRPRG(txn)->segment_count);
  nr_php_zval_free(&expr);
#endif
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

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_add_segment_metric(TSRMLS_C);
  test_txn_restart_in_callstack(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
