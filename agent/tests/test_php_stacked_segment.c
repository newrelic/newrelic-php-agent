/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_stacked_segment.h"
#include "php_globals.h"
#include "php_wrapper.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
static void test_start_end_discard(TSRMLS_D) {
  nr_segment_t* stacked = NULL;
  nr_segment_t* segment;

  tlib_php_request_start();

  /*
   * Initial state: current segment forced to root
   */
  tlib_pass_if_ptr_equal("current stacked segment forced to root",
                         NRTXN(segment_root), NRTXN(force_current_segment));

  /*
   * Add a stacked segment.
   */
  stacked = nr_php_stacked_segment_init(stacked);

  tlib_pass_if_not_null("current stacked forced to stacked should not be null",
                        stacked);
  tlib_pass_if_ptr_equal("current stacked segment has txn", stacked->txn,
                         NRPRG(txn));
  tlib_pass_if_ptr_equal("current stacked forced to stacked", stacked,
                         NRTXN(force_current_segment));

  /*
   * Discard a stacked segment.
   */
  nr_php_stacked_segment_deinit(stacked);

  tlib_pass_if_ptr_equal("current stacked segment forced to root",
                         NRTXN(segment_root), NRTXN(force_current_segment));
  tlib_pass_if_size_t_equal(
      "no segment created", 0,
      nr_segment_children_size(&NRTXN(segment_root)->children));

  /*
   * Add another stacked segment.
   */
  stacked = nr_php_stacked_segment_init(stacked TSRMLS_CC);

  tlib_pass_if_ptr_equal("current stacked segment has txn", stacked->txn,
                         NRPRG(txn));
  tlib_pass_if_ptr_equal("current stacked forced to stacked", stacked,
                         NRTXN(force_current_segment));

  /*
   * End a stacked segment.
   */
  segment = nr_php_stacked_segment_move_to_heap(stacked TSRMLS_CC);
  nr_segment_end(&segment);

  tlib_pass_if_true("moved segment is different from stacked segment",
                    segment != stacked, "%p!=%p", segment, stacked);
  tlib_pass_if_ptr_equal("current stacked segment forced to root",
                         NRTXN(segment_root), NRTXN(force_current_segment));
  tlib_pass_if_size_t_equal(
      "no segment created", 1,
      nr_segment_children_size(&NRTXN(segment_root)->children));

  tlib_php_request_end();
}

static void test_unwind(TSRMLS_D) {
  nr_segment_t* stacked_1 = NULL;
  nr_segment_t* stacked_2 = NULL;
  nr_segment_t* stacked_3 = NULL;
  nr_segment_t* segment;

  tlib_php_request_start();

  /*
   * Add stacked segments.
   */
  stacked_1 = nr_php_stacked_segment_init(stacked_1 TSRMLS_CC);
  stacked_2 = nr_php_stacked_segment_init(stacked_2 TSRMLS_CC);
  stacked_3 = nr_php_stacked_segment_init(stacked_3 TSRMLS_CC);

  /*
   * Add a regular segment.
   */
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  nr_segment_end(&segment);

  /*
   * Unwind the stacked segment stack.
   */
  nr_php_stacked_segment_unwind(TSRMLS_C);

  tlib_pass_if_size_t_equal(
      "one child segment of root", 1,
      nr_segment_children_size(&NRTXN(segment_root)->children));

  tlib_pass_if_size_t_equal("4 segments in total ", 4, NRTXN(segment_count));

  tlib_php_request_end();
}
#else
static void test_start_end_discard(TSRMLS_D) {
  nr_segment_t stacked = {0};
  nr_segment_t* segment;

  tlib_php_request_start();

  /*
   * Initial state: current segment forced to root
   */
  tlib_pass_if_ptr_equal("current stacked segment forced to root",
                         NRTXN(segment_root), NRTXN(force_current_segment));

  /*
   * Add a stacked segment.
   */
  nr_php_stacked_segment_init(&stacked TSRMLS_CC);

  tlib_pass_if_ptr_equal("current stacked segment has txn", stacked.txn,
                         NRPRG(txn));
  tlib_pass_if_ptr_equal("current stacked forced to stacked", &stacked,
                         NRTXN(force_current_segment));

  /*
   * Discard a stacked segment.
   */
  nr_php_stacked_segment_deinit(&stacked TSRMLS_CC);

  tlib_pass_if_ptr_equal("current stacked segment forced to root",
                         NRTXN(segment_root), NRTXN(force_current_segment));
  tlib_pass_if_size_t_equal(
      "no segment created", 0,
      nr_segment_children_size(&NRTXN(segment_root)->children));

  /*
   * Add another stacked segment.
   */
  nr_php_stacked_segment_init(&stacked TSRMLS_CC);

  tlib_pass_if_ptr_equal("current stacked segment has txn", stacked.txn,
                         NRPRG(txn));
  tlib_pass_if_ptr_equal("current stacked forced to stacked", &stacked,
                         NRTXN(force_current_segment));

  /*
   * End a stacked segment.
   */
  segment = nr_php_stacked_segment_move_to_heap(&stacked TSRMLS_CC);
  nr_segment_end(&segment);

  tlib_pass_if_true("moved segment is different from stacked segment",
                    segment != &stacked, "%p!=%p", segment, &stacked);
  tlib_pass_if_ptr_equal("current stacked segment forced to root",
                         NRTXN(segment_root), NRTXN(force_current_segment));
  tlib_pass_if_size_t_equal(
      "no segment created", 1,
      nr_segment_children_size(&NRTXN(segment_root)->children));

  tlib_php_request_end();
}

static void test_unwind(TSRMLS_D) {
  nr_segment_t stacked_1 = {0};
  nr_segment_t stacked_2 = {0};
  nr_segment_t stacked_3 = {0};
  nr_segment_t* segment;

  tlib_php_request_start();

  /*
   * Add stacked segments.
   */
  nr_php_stacked_segment_init(&stacked_1 TSRMLS_CC);
  nr_php_stacked_segment_init(&stacked_2 TSRMLS_CC);
  nr_php_stacked_segment_init(&stacked_3 TSRMLS_CC);

  /*
   * Add a regular segment.
   */
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  nr_segment_end(&segment);

  /*
   * Unwind the stacked segment stack.
   */
  nr_php_stacked_segment_unwind(TSRMLS_C);

  tlib_pass_if_size_t_equal(
      "one child segment of root", 1,
      nr_segment_children_size(&NRTXN(segment_root)->children));

  tlib_pass_if_size_t_equal("4 segments in total ", 4, NRTXN(segment_count));

  tlib_php_request_end();
}
#endif

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_start_end_discard(TSRMLS_C);
  test_unwind(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
