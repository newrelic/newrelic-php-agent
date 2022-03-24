/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains basic sanity checks for trace and entity metadata API
 * calls: newrelic_get_trace_metadata() newrelic_get_linking_metadata()
 *   newrelic_is_sampled()
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_api.h"
#include "php_hash.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_is_sampled(TSRMLS_D) {
  zval* retval;

  tlib_php_request_start();

  retval = nr_php_call(NULL, "newrelic_is_sampled");
  tlib_pass_if_zval_is_bool_value("newrelic_is_sampled() returns a bool ", 0,
                                  retval);

  nr_php_zval_free(&retval);
  tlib_php_request_end();
}

static void test_get_linking_metadata(TSRMLS_D) {
  zval* retval;
  zval* val;

  tlib_php_request_start();

  retval = nr_php_call(NULL, "newrelic_get_linking_metadata");

  tlib_pass_if_zval_type_is("newrelic_get_linking_metadata() returns an array ",
                            IS_ARRAY, retval);

  val = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "entity.type");
  tlib_pass_if_not_null("entity.name", val);
  tlib_pass_if_zval_type_is("entity.type", IS_STRING, val);
  tlib_pass_if_str_equal("entity.type", Z_STRVAL_P(val), "SERVICE");

  val = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "entity.name");
  tlib_pass_if_not_null("entity.name", val);
  tlib_pass_if_zval_type_is("entity.name", IS_STRING, val);

  val = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "hostname");
  tlib_pass_if_not_null("hostname", val);
  tlib_pass_if_zval_type_is("hostname", IS_STRING, val);

  /*
   * No trace.id and span.id, as DT is disabled by default
   */
  val = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "trace.id");
  tlib_pass_if_null("trace.id", val);

  val = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "span.id");
  tlib_pass_if_null("span.id", val);

  nr_php_zval_free(&retval);

  tlib_php_request_end();
}

static void test_get_trace_metadata(TSRMLS_D) {
  zval* retval;

  tlib_php_request_start();

  retval = nr_php_call(NULL, "newrelic_get_trace_metadata");

  tlib_pass_if_zval_type_is("newrelic_get_trace_metadata() returns an array ",
                            IS_ARRAY, retval);

  tlib_pass_if_size_t_equal("empty trace metadata", 0,
                            nr_php_zend_hash_num_elements(Z_ARRVAL_P(retval)));

  nr_php_zval_free(&retval);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("newrelic.distributed_tracing_enabled = false\n" PTSRMLS_CC);

  test_is_sampled(TSRMLS_C);
  test_get_linking_metadata(TSRMLS_C);
  test_get_trace_metadata(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
