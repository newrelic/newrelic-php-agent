/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_hash.h"
#include "php_zval.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

static void test_stack_trace_limit(TSRMLS_D) {
  char* json = NULL;
  tlib_php_request_start();
  zval* mock_trace = nr_php_zval_alloc();
  zval* temp_frame;
  zval* line_number = nr_php_zval_alloc();
  nrobj_t* trace_array;

  array_init(mock_trace);

  for (int i = 0; i < 300; i++) {
    temp_frame = nr_php_zval_alloc();
    array_init(temp_frame);
    ZVAL_LONG(line_number, i);
    nr_php_add_assoc_zval(temp_frame, "line", line_number);
    nr_php_add_assoc_string(temp_frame, "file", "throw.php");
    nr_php_add_assoc_string(temp_frame, "function", "throw_something");
    nr_php_add_assoc_string(temp_frame, "class", "a_class");
    nr_php_add_index_zval(mock_trace, i, temp_frame);
    nr_php_zval_free(&temp_frame);
  }

  temp_frame = nr_php_zval_alloc();
  array_init(temp_frame);
  ZVAL_LONG(line_number, 300);
  nr_php_add_assoc_zval(temp_frame, "line", line_number);
  nr_php_add_assoc_string(temp_frame, "file", "12345678");
  nr_php_add_assoc_string(temp_frame, "function", "someFunc");
  nr_php_add_assoc_string(temp_frame, "class", "i_shouldnt_be_here");
  nr_php_add_index_zval(mock_trace, 300, temp_frame);
  nr_php_zval_free(&temp_frame);

  json = nr_php_backtrace_to_json(mock_trace TSRMLS_CC);

  /*
   * Test : Stack traces should not contain more than 300 lines and should
   * truncate the end.
   */
  tlib_pass_if_null("should truncate", nr_strstr(json, "12345678"));

  /*
   * Test : The stack trace should still contain the first line.
   */
  tlib_pass_if_not_null(
      "The stack trace should still exist",
      nr_strstr(json, "a_class::throw_something called at throw.php (0)"));

  /*
   * Test : The stack trace should still contain the last line.
   */
  tlib_pass_if_not_null(
      "The stack trace should still exist",
      nr_strstr(json, "a_class::throw_something called at throw.php (299)"));

  /*
   * Test : A message was given to the user indicating how many lines were
   * removed.
   */
  tlib_pass_if_not_null("1 line was removed",
                        nr_strstr(json,
                                  "*** The stack trace was truncated here - 1 "
                                  "line(s) were removed ***"));

  trace_array = nro_create_from_json(json);

  tlib_pass_if_true(__func__, nro_getsize(trace_array) == 301,
                    "The trace should be exactly 301 lines, but is - %d",
                    nro_getsize(trace_array));

  nr_free(json);
  nr_php_zval_free(&line_number);
  nr_php_zval_free(&mock_trace);
  nro_delete(trace_array);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_stack_trace_limit(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
