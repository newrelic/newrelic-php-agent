/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_segment.h"
#include "nr_segment_children.h"

nr_segment_t* nr_segment_children_get_prev(nr_segment_children_t* children,
                                           nr_segment_t* child) {
  nr_segment_t* prev;
  nr_segment_t* cur;
  size_t i;
  size_t used;

  if (nrunlikely(NULL == children || NULL == child)) {
    return NULL;
  }

  /* If there are 1 or fewer children, there cannot be a child previous to the
   * one given */
  used = nr_segment_children_size(children);
  if (1 >= used) {
    return NULL;
  }

  for (i = 1; i < used; i++) {
    prev = nr_segment_children_get(children, i - 1);
    cur = nr_segment_children_get(children, i);

    if (cur == child) {
      return prev;
    }
  }

  /* The supplied child is not among the children */
  return NULL;
}

nr_segment_t* nr_segment_children_get_next(nr_segment_children_t* children,
                                           nr_segment_t* child) {
  nr_segment_t* cur;
  nr_segment_t* next;
  size_t i;
  size_t used;

  if (nrunlikely(NULL == children || NULL == child)) {
    return NULL;
  }

  /* If there are 1 or fewer children, there cannot be a child after the
   * one given */
  used = nr_segment_children_size(children);
  if (1 >= used) {
    return NULL;
  }

  for (i = 0; i < (used - 1); i++) {
    cur = nr_segment_children_get(children, i);
    next = nr_segment_children_get(children, i + 1);

    if (cur == child) {
      return next;
    }
  }

  /* The supplied child is not among the children */
  return NULL;
}

bool nr_segment_children_reparent(nr_segment_children_t* children,
                                  nr_segment_t* new_parent) {
  size_t i;
  size_t parent_size;
  size_t req_parent_size;
  size_t size;
  void* source;

  if (nrunlikely(NULL == children || NULL == new_parent)) {
    return false;
  }

  size = nr_segment_children_size(children);
  if (0 == size) {
    // Do nothing, successfully.
    return true;
  }

  for (i = 0; i < size; i++) {
    nr_segment_t* child = nr_segment_children_get(children, i);

    child->parent = new_parent;
  }

  parent_size = nr_segment_children_size(&new_parent->children);
  req_parent_size = size + parent_size;
  if (req_parent_size > NR_SEGMENT_CHILDREN_PACKED_LIMIT) {
    nr_segment_children_migrate_to_vector(&new_parent->children);
  }

  if (children->is_packed) {
    source = &children->packed.elements[0];
  } else {
    source = &children->vector.elements[0];
  }

  if (new_parent->children.is_packed) {
    nr_memcpy(&new_parent->children.packed.elements[parent_size], source,
              size * sizeof(nr_segment_t*));
    new_parent->children.packed.count = req_parent_size;
  } else {
    nr_vector_ensure(&new_parent->children.vector, req_parent_size);
    nr_memcpy(&new_parent->children.vector.elements[parent_size], source,
              size * sizeof(nr_segment_t*));
    new_parent->children.vector.used = req_parent_size;
  }

  nr_segment_children_deinit(children);
  return true;
}
