/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_api_internal.h"

/*
 * This file contains basic sanity checks for newrelic_get_trace_json(), but
 * makes no effort to fully validate any traces that are returned. The axiom
 * unit tests perform this task extremely thoroughly, and doing so here would be
 * a duplicated, wasted effort.
 */

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_empty(TSRMLS_D) {
  const nrobj_t* details;
  nrobj_t* obj;
  zval* retval;

  tlib_php_request_start();

  retval = nr_php_call(NULL, "newrelic_get_trace_json");

  tlib_pass_if_zval_type_is("newrelic_get_trace_json() returns a string",
                            IS_STRING, retval);

  obj = nro_create_from_json(Z_STRVAL_P(retval));
  tlib_pass_if_not_null("that string should be valid JSON", obj);

  tlib_pass_if_int_equal("the trace top level should be an array",
                         (int)NR_OBJECT_ARRAY, (int)nro_type(obj));
  tlib_pass_if_int_equal("that array should have two elements", 2,
                         nro_getsize(obj));

  details = nro_get_array_array(obj, 1, NULL);
  tlib_pass_if_not_null("the trace details should be an array", details);
  tlib_pass_if_int_equal("the trace details should have five elements", 5,
                         nro_getsize(details));

  nro_delete(obj);
  nr_php_zval_free(&retval);
  tlib_php_request_end();
}

static void test_invalid_parameters(TSRMLS_D) {
  zval* param = nr_php_zval_alloc();
  zval* retval = NULL;

  tlib_php_request_start();

  /* Literally any parameter should cause this to bail. */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  tlib_php_request_eval(
      "$exception = false;"
      "try {"
      "    $value = newrelic_get_trace_json('invalid');"
      "    echo \"No exception, returned \" . $value . \".\\n\";"
      "} catch(ArgumentCountError $_e) {"
      "    $exception = true;"
      "}");
  retval = tlib_php_request_eval_expr("$exception;" TSRMLS_CC);

  tlib_pass_if_zval_is_bool_true(
      "newrelic_get_trace_json() throws an exception when a parameter is given",
      retval);
#else
  retval = nr_php_call(NULL, "newrelic_get_trace_json", param);

  tlib_pass_if_zval_is_bool_false(
      "newrelic_get_trace_json() returns false when a parameter is given",
      retval);
#endif

  nr_php_zval_free(&param);
  nr_php_zval_free(&retval);
  tlib_php_request_end();
}

static void test_not_recording(TSRMLS_D) {
  zval* retval = NULL;

  tlib_php_request_start();

  NRPRG(txn)->status.recording = false;
  retval = nr_php_call(NULL, "newrelic_get_trace_json");

  tlib_pass_if_zval_is_bool_false(
      "newrelic_get_trace_json() returns false when the transaction is not "
      "recording",
      retval);

  nr_php_zval_free(&retval);
  tlib_php_request_end();
}

static void test_segments(TSRMLS_D) {
  const nrobj_t* details;
  nrobj_t* obj;
  zval* retval;
  const nrobj_t* string_table;

  tlib_php_request_start();

  tlib_php_request_eval(
      "function f() { time_nanosleep(0, 2000 * 1000); }"
      "function g() { return newrelic_get_trace_json(); }"
      "f(); f();" TSRMLS_CC);

  /*
   * Ensure we call newrelic_get_trace_json() from within a user function,
   * thereby checking that all active segments were temporarily stopped.
   */
  retval = tlib_php_request_eval_expr("g()" TSRMLS_CC);

  tlib_pass_if_zval_type_is("newrelic_get_trace_json() returns a string",
                            IS_STRING, retval);

  obj = nro_create_from_json(Z_STRVAL_P(retval));
  tlib_pass_if_not_null("that string should be valid JSON", obj);

  tlib_pass_if_int_equal("the trace top level should be an array",
                         (int)NR_OBJECT_ARRAY, (int)nro_type(obj));
  tlib_pass_if_int_equal("that array should have two elements", 2,
                         nro_getsize(obj));

  details = nro_get_array_array(obj, 1, NULL);
  tlib_pass_if_not_null("the trace details should be an array", details);
  tlib_pass_if_int_equal("the trace details should have five elements", 5,
                         nro_getsize(details));

  /*
   * The string table only contains two elements because, at the point the trace
   * was generated, the segment for g() had not yet been named and is therefore
   * <unknown>.
   */
  string_table = nro_get_array_array(obj, 2, NULL);
  tlib_pass_if_not_null("the string table should be an array", string_table);
  tlib_pass_if_int_equal("the string table should have two elements", 2,
                         nro_getsize(string_table));

  nro_delete(obj);
  nr_php_zval_free(&retval);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_not_recording(TSRMLS_C);
  test_invalid_parameters(TSRMLS_C);
  test_empty(TSRMLS_C);
  test_segments(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
