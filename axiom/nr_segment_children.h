/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The segment children API.
 *
 * Most functions in this API are declared static inline within the header so
 * that the compiler may aggressively inline.
 */
#ifndef NR_SEGMENT_CHILDREN_HDR
#define NR_SEGMENT_CHILDREN_HDR

#include <stdbool.h>

#define NR_SEGMENT_CHILDREN_PACKED_LIMIT 8

#include "util_memory.h"
#include "util_vector.h"

/*
 * Forward declaration of nr_segment_t, since we have a circular dependency with
 *  nr_segment.h.
 */
typedef struct _nr_segment_t nr_segment_t;

/*
 * The data structure for packed children, holding an array of children and the
 * number of elements in the array.
 */
typedef struct {
  size_t count;
  nr_segment_t* elements[NR_SEGMENT_CHILDREN_PACKED_LIMIT];
} nr_segment_packed_children_t;

/*
 * The children structure. If `is_packed` is true the union is used as packed,
 * otherwise it is used as vector.
 */
typedef struct {
  bool is_packed;
  union {
    nr_vector_t vector;
    nr_segment_packed_children_t packed;
  };
} nr_segment_children_t;

#include "nr_segment_children_private.h"

/*
 * Purpose : Initialize a segment's children.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 */
static inline void nr_segment_children_init(nr_segment_children_t* children) {
  if (nrunlikely(NULL == children)) {
    return;
  }

  children->is_packed = true;
  children->packed.count = 0;
}

/*
 * Purpose : Deinitialise a segment's children.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 */
static inline void nr_segment_children_deinit(nr_segment_children_t* children) {
  if (nrunlikely(NULL == children)) {
    return;
  }

  if (!children->is_packed) {
    nr_vector_deinit(&children->vector);
  }
  nr_segment_children_init(children);
}

/*
 * Purpose : Return the number of children.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *
 * Returns : The number of children, or 0 on error.
 */
static inline size_t nr_segment_children_size(
    const nr_segment_children_t* children) {
  if (nrunlikely(NULL == children)) {
    return 0;
  }

  return children->is_packed ? children->packed.count
                             : nr_vector_size(&children->vector);
}

/*
 * Purpose : Return a child within a segment.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *           2. The index of the child to return.
 *
 * Returns : The child element, or NULL on error.
 */
static inline nr_segment_t* nr_segment_children_get(
    nr_segment_children_t* children,
    size_t i) {
  // The nr_segment_children_size() call will also implicitly check for NULL.
  if (nrunlikely(i >= nr_segment_children_size(children))) {
    return NULL;
  }

  // This breaks the encapsulation of nr_vector_t, but avoids the checks that
  // we've already done above.
  return children->is_packed ? children->packed.elements[i]
                             : children->vector.elements[i];
}

/*
 * Purpose : Add a child to a segment's children.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *           2. A pointer to the segment to add.
 *
 * Returns : True if successful, false otherwise.
 */
static inline bool nr_segment_children_add(nr_segment_children_t* children,
                                           nr_segment_t* child) {
  if (nrunlikely(NULL == children || NULL == child)) {
    return false;
  }

  if (children->is_packed) {
    size_t new_count = children->packed.count + 1;

    if (new_count > NR_SEGMENT_CHILDREN_PACKED_LIMIT) {
      // We're about to overflow the packed array; migrate to a vector.
      nr_segment_children_migrate_to_vector(children);
      nr_segment_children_add_vector(children, child);
    } else {
      children->packed.elements[children->packed.count] = child;
      children->packed.count = new_count;
    }
  } else {
    nr_segment_children_add_vector(children, child);
  }

  return true;
}

/*
 * Purpose : Remove a child from a segment's children.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *           2. A pointer to the segment to remove.
 *
 * Returns : True if successful, false otherwise.
 */
static inline bool nr_segment_children_remove(nr_segment_children_t* children,
                                              const nr_segment_t* child) {
  if (nrunlikely(NULL == children || NULL == child
                 || 0 == nr_segment_children_size(children))) {
    return false;
  }

  if (children->is_packed) {
    size_t i;
    const size_t end = children->packed.count - 1;

    if (child == children->packed.elements[end]) {
      // The simple case: the child is the last element, so we can just
      // decrement the count and we're done.
      children->packed.count -= 1;
      return true;
    }

    for (i = 0; i < end; i++) {
      if (child == children->packed.elements[i]) {
        nr_memmove(&children->packed.elements[i],
                   &children->packed.elements[i + 1],
                   sizeof(nr_segment_t*) * (children->packed.count - i - 1));
        children->packed.count -= 1;
        return true;
      }
    }
  } else {
    size_t index;

    if (nr_vector_find_first(&children->vector, child, NULL, NULL, &index)) {
      void* element;

      return nr_vector_remove(&children->vector, index, &element);
    }
  }

  return false;
}

/*
 * Purpose : Reparent all children onto a new parent.
 *
 * Params  : 1. A pointer to a segment's nr_segment_children_t structure.
 *           2. The new parent segment.
 *
 * Returns : True on success; false otherwise.
 */
extern bool nr_segment_children_reparent(nr_segment_children_t* children,
                                         nr_segment_t* new_parent);

#endif /* NR_SEGMENT_CHILDREN_HDR */
