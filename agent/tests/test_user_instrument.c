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
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC),
                         &func);
#else
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(func), &func);
#endif

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  /*
   * Invalidate the cached pid.
   */
  NRPRG(pid) -= 1;
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC), NULL)
#else
  /*
   * Not NULL because we don't care about the reserved array anymore.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(func), &func);
#endif
      /*
       * Restore the cached pid and invalidate the mangled pid/index value.
       */
      NRPRG(pid)
      += 1;

  {
    unsigned long pval;

    pval = (unsigned long)oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)];
    oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = (void*)(pval * 2);
  }

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC), NULL)
#else
  /*
   * Not NULL because we don't care about the reserved array anymore.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_name(func), &func);
#endif
#endif /* PHP >= 7.3 */

      tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_op_array_wraprec(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
