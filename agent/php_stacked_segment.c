/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_stacked_segment.h"

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

  stacked->txn = NRPRG(txn);
  NR_PHP_CURRENT_STACKED_PUSH(stacked);
  stacked->start_time = nr_txn_now_rel(stacked->txn);

  nr_segment_children_init(&stacked->children);

  return stacked;
}

void nr_php_stacked_segment_deinit(nr_segment_t* stacked TSRMLS_DC) {
  if (NULL == NRPRG(txn)) {
    return;
  }

  nr_segment_children_reparent(&stacked->children, stacked->parent);

  nr_free(stacked->id);

  NR_PHP_CURRENT_STACKED_POP(stacked);
}

void nr_php_stacked_segment_unwind(TSRMLS_D) {
  nr_segment_t* stacked;
  nr_segment_t* segment;

  if (NULL == NRPRG(txn)) {
    return;
  }

  while (NRTXN(force_current_segment)
         && (NRTXN(segment_root) != NRTXN(force_current_segment))) {
    stacked = NRTXN(force_current_segment);
    segment = nr_php_stacked_segment_move_to_heap(stacked TSRMLS_CC);
    nr_segment_end(&segment);
  }
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

  return s;
}
