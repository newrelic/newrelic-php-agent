/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_stacked_segment.h"
#include "util_logging.h"
#include "php_execute.h"
#include "php_error.h"

/*
 * Purpose : Add a stacked segment to the stacked segment stack. The top
 *           of the stack is stored in NRTXN(force_current_segment).
 */
#define NR_PHP_CURRENT_STACKED_PUSH(_stacked)        \
  (_stacked)->parent = NRTXN(force_current_segment); \
  NRTXN(force_current_segment) = (_stacked)

/*
 * Purpose : Pop a stacked segment from the stacked segment stack.
 */
#define NR_PHP_CURRENT_STACKED_POP(_stacked)                             \
  if (NRTXN(force_current_segment) == _stacked) {                        \
    NRTXN(force_current_segment) = NRTXN(force_current_segment)->parent; \
  }

nr_segment_t* nr_php_stacked_segment_init(nr_segment_t* stacked TSRMLS_DC) {
  if (!nr_php_recording(TSRMLS_C)) {
    return NULL;
  }
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  stacked = nr_calloc(1, sizeof(nr_segment_t));
  if (NULL == stacked) {
    return NULL;
  }

  stacked->metadata = nr_calloc(1, sizeof(nr_php_execute_metadata_t));
  if (NULL == stacked->metadata) {
    nr_free(stacked);
    return NULL;
  }

#endif

  stacked->txn = NRPRG(txn);
  NR_PHP_CURRENT_STACKED_PUSH(stacked);
  stacked->start_time = nr_txn_now_rel(stacked->txn);

  nr_segment_children_init(&stacked->children);

  return stacked;
}

void nr_php_stacked_segment_deinit(nr_segment_t* stacked TSRMLS_DC) {
  if (NULL == NRPRG(txn) || (NULL == stacked)) {
    return;
  }
  nr_segment_children_reparent(&stacked->children, stacked->parent);

  nr_free(stacked->id);

  NR_PHP_CURRENT_STACKED_POP(stacked);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  /*
   * This is allocated differently for OAPI and hence needs to be freed.
   */
  nr_php_execute_metadata_release(stacked->metadata);
  nr_free(stacked->metadata);
  nr_free(stacked);
#endif
}

void nr_php_stacked_segment_unwind(TSRMLS_D) {
  if (NULL == NRPRG(txn)) {
    return;
  }

  while (NRTXN(force_current_segment)
         && (NRTXN(segment_root) != NRTXN(force_current_segment))) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
    /*
     * With OAPI, we need to gracefully close off the stacked segments with
     * their naming contexts.
     */
    nr_php_observer_segment_end(NRPRG(uncaught_exception));

#else
    stacked = NRTXN(force_current_segment);
    segment = nr_php_stacked_segment_move_to_heap(stacked TSRMLS_CC);
    nr_segment_end(&segment);

#endif
  }
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  /*
   * If OAPI we need to record the uncaught exception (if it exists) on the root
   * segment as well.
   */
  if (NULL != NRPRG(uncaught_exception)) {
    if (NRTXN(segment_root) == NRTXN(force_current_segment)) {
      nr_php_error_record_exception_segment(
          NRPRG(txn), NRPRG(uncaught_exception),
          &NRPRG(exception_filters) TSRMLS_CC);
    }
  }
  php_observer_clear_uncaught_exception_globals();
#endif
}

nr_segment_t* nr_php_stacked_segment_move_to_heap(
    nr_segment_t* stacked TSRMLS_DC) {
  nr_segment_t* s;
  size_t i;

  s = nr_txn_allocate_segment(NRPRG(txn));

  if (nrunlikely(NULL == s)) {
    return NULL;
  }

  nr_memcpy(s, stacked, sizeof(*stacked));

  for (i = 0; i < nr_segment_children_size(&s->children); i++) {
    nr_segment_t* child = nr_segment_children_get(&s->children, i);
    if (nrlikely(child)) {
      child->parent = s;
    }
  }

  s->parent = NULL;
  nr_segment_set_parent(s, stacked->parent);

  NR_PHP_CURRENT_STACKED_POP(stacked);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */
  /*
   * This is allocated differently for OAPI and hence needs to be freed.
   */
  nr_php_execute_metadata_release(stacked->metadata);
  nr_free(stacked->metadata);
  nr_free(stacked);
#endif

  return s;
}
