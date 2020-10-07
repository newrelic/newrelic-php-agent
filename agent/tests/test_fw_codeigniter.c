/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_internal_instrument.h"
#include "fw_codeigniter.h"

static void cufa_pre_callback(zend_function* func NRUNUSED,
                              const zend_function* caller NRUNUSED TSRMLS_DC) {
  zend_op_array* op_array = nr_codeigniter_get_topmost_user_op_array(TSRMLS_C);

  tlib_pass_if_not_null("the op array must be non-NULL", op_array);
  tlib_pass_if_str_equal("the filename must be -", "-",
                         nr_php_op_array_file_name(op_array));
}

static void invoke_cufa(TSRMLS_D) {
  nr_php_add_call_user_func_array_pre_callback(cufa_pre_callback TSRMLS_CC);

  tlib_php_request_eval("function cufa_target() {}" TSRMLS_CC);
  tlib_php_request_eval(
      "call_user_func_array('cufa_target', array());" TSRMLS_CC);
}

static void test_get_topmost_user_op_array(TSRMLS_D) {
#ifdef PHP7
  /*
   * First, we'll test this with call_user_func_array() inlining.
   */
  tlib_php_request_start();
  CG(compiler_options) &= ~ZEND_COMPILE_NO_BUILTINS;
  invoke_cufa(TSRMLS_C);
  tlib_php_request_end();

  /*
   * Now we'll test this without call_user_func_array() inlining.
   */
  tlib_php_request_start();
  CG(compiler_options) |= ZEND_COMPILE_NO_BUILTINS;
  invoke_cufa(TSRMLS_C);
  tlib_php_request_end();
#else
  tlib_php_request_start();
  invoke_cufa(TSRMLS_C);
  tlib_php_request_end();
#endif /* PHP7 */
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);
  test_get_topmost_user_op_array(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
