/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_datastore.h"
#include "tlib_main.h"
#include "tlib_php.h"

#include "nr_datastore.h"
#include "php_agent.h"
#include "php_hash.h"

static void test_del(void) {
  zval* zv = nr_php_zval_alloc();

  array_init(zv);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL hash table", 0,
                         nr_php_zend_hash_del(NULL, "key"));
  tlib_pass_if_int_equal("NULL key", 0,
                         nr_php_zend_hash_del(Z_ARRVAL_P(zv), NULL));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal("key doesn't exist", 0,
                         nr_php_zend_hash_del(Z_ARRVAL_P(zv), "key"));

  nr_php_add_assoc_string(zv, "key", "value");
  tlib_fail_if_int_equal("key exists", 0,
                         nr_php_zend_hash_del(Z_ARRVAL_P(zv), "key"));
  tlib_pass_if_int_equal("key no longer exists", 0,
                         nr_php_zend_hash_exists(Z_ARRVAL_P(zv), "key"));

  tlib_pass_if_int_equal("key doesn't exist again", 0,
                         nr_php_zend_hash_del(Z_ARRVAL_P(zv), "key"));

  nr_php_zval_free(&zv);
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);
  tlib_php_request_start();

  test_del();

  tlib_php_request_end();
  tlib_php_engine_destroy(TSRMLS_C);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
