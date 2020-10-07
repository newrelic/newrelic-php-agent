/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_MINMAX_HEAP_PRIVATE_HDR
#define UTIL_MINMAX_HEAP_PRIVATE_HDR

/*
 * As NULL can be a valid value, we need a helper structure to track whether an
 * element has actually been used or not.
 */
typedef struct _nr_minmax_heap_element_t {
  bool used;
  void* value;
} nr_minmax_heap_element_t;

/*
 * The actual min-max heap structure.
 */
typedef struct _nr_minmax_heap_t {
  // If 0, the heap is considered unbounded and will be grown as necessary.
  ssize_t bound;
  ssize_t capacity;
  ssize_t used;

  // The actual element array.
  nr_minmax_heap_element_t* elements;

  nr_minmax_heap_cmp_t comparator;
  void* comparator_userdata;

  nr_minmax_heap_dtor_t destructor;
  void* destructor_userdata;
} nr_minmax_heap_t;

/*
 * The default size of each chunk when initially allocating and expanding an
 * unbounded min-max heap. This number is pulled more or less straight from thin
 * air, and should probably be configurable at create time.
 */
static const ssize_t NR_MINMAX_HEAP_CHUNK_SIZE = 64;

#endif /* UTIL_MINMAX_HEAP_PRIVATE_HDR */
