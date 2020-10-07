/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_math.h"
#include "util_memory.h"
#include "util_minmax_heap.h"
#include "util_minmax_heap_private.h"

/*
 * Copyright (c) 2015 Marco Zavagno
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * This is derived from min-max-heap by Marco "Banshee" Zavagno, which is MIT
 * licensed.
 *
 * Source: https://github.com/ilbanshee/min-max-heap
 *
 * The underlying data structure was first described by Atkinson, Sack, Santoro,
 * and Strothotte (1986):
 *
 * https://cglab.ca/~morin/teaching/5408/refs/minmax.pdf
 *
 * At a high level, the changes are mostly to fit our house style and for
 * readability. Macros have been converted to inline statics. Indexes are
 * ssize_t instead of signed ints. Exported functions are prefixed to avoid
 * symbol clashes.
 *
 * The only functional changes are the ability to bound a heap by setting a
 * capacity, support for custom comparison functions, and support for optional
 * destructors invoked when elements are removed at destroy time, or evicted due
 * to the heap being bounded.
 */

/*
 * Basic index helpers used to traverse the embedded tree within the array that
 * forms the primary data structure. These are all simple operations, but having
 * named functions (or macros, in the upstream library) is better for
 * readability.
 */
static inline bool is_min(ssize_t n) {
  if (n < 0) {
    return false;
  }

  /*
   * The tree contains min layers on even tree depths, starting at the root, and
   * max layers on odd tree depths.
   */
  return (nr_log2_64((uint64_t)n) & 1) == 0;
}

static inline ssize_t parent(ssize_t n) {
  return n / 2;
}

static inline ssize_t first_child(ssize_t n) {
  return n * 2;
}

static inline ssize_t second_child(ssize_t n) {
  return (n * 2) + 1;
}

/*
 * Internal operations.
 *
 * The bubble-up and trickle-down operations have been refactored into common
 * functions, with shims with clearer function names for readability. The
 * compiler should be smart enough to inline where necessary.
 */
static int compare(const nr_minmax_heap_t* heap, ssize_t i, ssize_t j) {
  return ((heap->comparator)(heap->elements[i].value, heap->elements[j].value,
                             heap->comparator_userdata));
}

typedef enum {
  COMPARE_LESS_THAN,
  COMPARE_GREATER_THAN,
  COMPARE_EQUAL,
} compare_t;

static int compare_expect(const nr_minmax_heap_t* heap,
                          ssize_t i,
                          ssize_t j,
                          compare_t op) {
  int result = compare(heap, i, j);

  switch (op) {
    case COMPARE_LESS_THAN:
      return result < 0;
    case COMPARE_GREATER_THAN:
      return result > 0;
    case COMPARE_EQUAL:
    default:
      return result == 0;
  }
}

static void swap(nr_minmax_heap_t* heap, ssize_t i, ssize_t j) {
  nr_minmax_heap_element_t tmp = heap->elements[i];

  heap->elements[i] = heap->elements[j];
  heap->elements[j] = tmp;
}

static void bubbleup_minmax(nr_minmax_heap_t* heap, ssize_t i, compare_t op) {
  ssize_t grandparent = parent(parent(i));

  if (grandparent <= 0) {
    return;
  }

  if (compare_expect(heap, i, grandparent, op)) {
    swap(heap, i, grandparent);
    bubbleup_minmax(heap, grandparent, op);
  }
}

static void bubbleup_min(nr_minmax_heap_t* heap, ssize_t i) {
  bubbleup_minmax(heap, i, COMPARE_LESS_THAN);
}

static void bubbleup_max(nr_minmax_heap_t* heap, ssize_t i) {
  bubbleup_minmax(heap, i, COMPARE_GREATER_THAN);
}

static void bubbleup(nr_minmax_heap_t* heap, ssize_t i) {
  ssize_t p = parent(i);

  if (p <= 0) {
    return;
  }

  if (is_min(i)) {
    if (compare(heap, i, p) > 0) {
      swap(heap, i, p);
      bubbleup_max(heap, p);
    } else {
      bubbleup_min(heap, i);
    }
  } else {
    if (compare(heap, i, p) < 0) {
      swap(heap, i, p);
      bubbleup_min(heap, p);
    } else {
      bubbleup_max(heap, i);
    }
  }
}

/* This function gets the minimum or maximum child or grandchild of the given
 * node. */
static ssize_t index_minmax_child_grandchild(const nr_minmax_heap_t* heap,
                                             ssize_t i,
                                             compare_t op) {
  ssize_t a = first_child(i);
  ssize_t b = second_child(i);
  ssize_t c = first_child(a);
  ssize_t d = second_child(a);
  ssize_t e = first_child(b);
  ssize_t f = second_child(b);
  ssize_t idx = -1;

  if (a <= heap->used) {
    idx = a;
  }
  if (b <= heap->used && compare_expect(heap, b, idx, op)) {
    idx = b;
  }
  if (c <= heap->used && compare_expect(heap, c, idx, op)) {
    idx = c;
  }
  if (d <= heap->used && compare_expect(heap, d, idx, op)) {
    idx = d;
  }
  if (e <= heap->used && compare_expect(heap, e, idx, op)) {
    idx = e;
  }
  if (f <= heap->used && compare_expect(heap, f, idx, op)) {
    idx = f;
  }

  return idx;
}

static void trickledown_minmax(nr_minmax_heap_t* heap,
                               ssize_t i,
                               compare_t op) {
  ssize_t m = index_minmax_child_grandchild(heap, i, op);

  if (m <= -1) {
    return;
  }

  if (m > second_child(i)) {
    // m is a grandchild
    if (compare_expect(heap, m, i, op)) {
      swap(heap, i, m);
      if (!compare_expect(heap, m, parent(m), op)) {
        swap(heap, m, parent(m));
      }
      trickledown_minmax(heap, m, op);
    }
  } else {
    // m is a child
    if (compare_expect(heap, m, i, op)) {
      swap(heap, i, m);
    }
  }
}

static void trickledown_max(nr_minmax_heap_t* heap, ssize_t i) {
  trickledown_minmax(heap, i, COMPARE_GREATER_THAN);
}

static void trickledown_min(nr_minmax_heap_t* heap, ssize_t i) {
  trickledown_minmax(heap, i, COMPARE_LESS_THAN);
}

static void trickledown(nr_minmax_heap_t* heap, ssize_t i) {
  if (is_min(i)) {
    trickledown_min(heap, i);
  } else {
    trickledown_max(heap, i);
  }
}

/*
 * Now the public API.
 */
nr_minmax_heap_t* nr_minmax_heap_create(ssize_t bound,
                                        nr_minmax_heap_cmp_t comparator,
                                        void* comparator_userdata,
                                        nr_minmax_heap_dtor_t destructor,
                                        void* destructor_userdata) {
  nr_minmax_heap_t* heap;

  if ((NULL == comparator) || (bound == 1) || (bound < 0)) {
    return NULL;
  }

  heap = (nr_minmax_heap_t*)nr_malloc(sizeof(nr_minmax_heap_t));

  heap->bound = bound;
  heap->capacity = bound > 0 ? bound + 1 : NR_MINMAX_HEAP_CHUNK_SIZE;
  heap->used = 0;
  // We'll rely on the zeroing behaviour of calloc() here to ensure that all
  // elements are marked as unused (as the used value will be 0).
  heap->elements = nr_calloc(heap->capacity, sizeof(nr_minmax_heap_element_t));
  heap->comparator = comparator;
  heap->comparator_userdata = comparator_userdata;
  heap->destructor = destructor;
  heap->destructor_userdata = destructor_userdata;

  return heap;
}

void nr_minmax_heap_destroy(nr_minmax_heap_t** heap_ptr) {
  nr_minmax_heap_t* heap;

  if ((NULL == heap_ptr) || (NULL == *heap_ptr)) {
    return;
  }

  heap = *heap_ptr;

  if (heap->destructor) {
    for (ssize_t i = 0; i <= heap->used; i++) {
      if (heap->elements[i].used) {
        (heap->destructor)(heap->elements[i].value, heap->destructor_userdata);
      }
    }
  }

  nr_free(heap->elements);
  nr_realfree((void**)heap_ptr);
}

ssize_t nr_minmax_heap_bound(const nr_minmax_heap_t* heap) {
  return heap ? heap->bound : 0;
}

ssize_t nr_minmax_heap_capacity(const nr_minmax_heap_t* heap) {
  return heap ? heap->capacity : 0;
}

ssize_t nr_minmax_heap_size(const nr_minmax_heap_t* heap) {
  return heap ? heap->used : 0;
}

void nr_minmax_heap_insert(nr_minmax_heap_t* heap, void* value) {
  if (NULL == heap) {
    return;
  }

  // If the heap is bounded and full, we need to figure out if we even want to
  // insert the value.
  if (heap->bound && heap->used >= heap->bound) {
    if ((heap->comparator)(nr_minmax_heap_peek_min(heap), value,
                           heap->comparator_userdata)
        > 0) {
      // The value is less than the minimum value in the heap currently; so
      // destroy the value and return.
      if (heap->destructor) {
        (heap->destructor)(value, heap->destructor_userdata);
      }

      return;
    } else {
      // The value is greater than the minimum value in the heap; evict that
      // value and create a space for the new value to be inserted.
      void* min = nr_minmax_heap_pop_min(heap);

      if (heap->destructor) {
        (heap->destructor)(min, heap->destructor_userdata);
      }
    }
  }

  // Otherwise, let's figure out if we need to grow the heap. Note that there's
  // a potential off by one error here: if the root of the heap at index 1 has
  // no left children, then we may have a wasted element at index 0. We need to
  // ensure that capacity is actually one higher than we think we need.
  if ((heap->used + 1) >= heap->capacity) {
    // This is the most naive growth strategy imaginable, so it'll probably work
    // out just fine.
    ssize_t new_capacity = heap->capacity * 2;
    ssize_t new_size_bytes = new_capacity * sizeof(nr_minmax_heap_element_t);
    nr_minmax_heap_element_t* new_elements;
    ssize_t old_size_bytes = heap->capacity * sizeof(nr_minmax_heap_element_t);

    // Actually perform the reallocation.
    new_elements = nr_realloc(heap->elements, new_size_bytes);

    // There really isn't much sensible we can do on failure. On Linux, at
    // least, this is unreachable with a normal kernel configuration.
    if (NULL == new_elements) {
      return;
    }

    // realloc() does not zero out the newly allocated memory, so let's ensure
    // we do that.
    nr_memset(new_elements + heap->capacity, 0,
              new_size_bytes - old_size_bytes);

    // Finally, update the heap structure.
    heap->capacity = new_capacity;
    heap->elements = new_elements;
  }

  // OK, now to actually add the element.
  heap->used++;
  heap->elements[heap->used].used = true;
  heap->elements[heap->used].value = value;
  bubbleup(heap, heap->used);
}

void* nr_minmax_heap_pop_min(nr_minmax_heap_t* heap) {
  void* value;

  if (NULL == heap || heap->used == 0) {
    return NULL;
  }

  if (heap->used == 1) {
    heap->used--;
    value = heap->elements[1].value;
    heap->elements[1].used = false;

    return value;
  }

  value = heap->elements[1].value;
  heap->elements[1] = heap->elements[heap->used];
  heap->elements[heap->used].used = false;
  heap->used--;
  trickledown(heap, 1);

  return value;
}

void* nr_minmax_heap_pop_max(nr_minmax_heap_t* heap) {
  ssize_t idx;
  void* value;

  if (NULL == heap || 0 == heap->used) {
    return NULL;
  }

  if (heap->used <= 2) {
    idx = heap->used;
    heap->used--;
    heap->elements[idx].used = false;
    return heap->elements[idx].value;
  }

  if (compare(heap, 2, 3) < 0) {
    idx = 3;
  } else {
    idx = 2;
  }
  value = heap->elements[idx].value;
  heap->elements[idx] = heap->elements[heap->used];
  heap->elements[heap->used].used = false;
  heap->used--;
  trickledown(heap, idx);

  return value;
}

const void* nr_minmax_heap_peek_min(const nr_minmax_heap_t* heap) {
  if (NULL == heap || 0 == heap->used) {
    return NULL;
  }

  return heap->elements[1].value;
}

const void* nr_minmax_heap_peek_max(const nr_minmax_heap_t* heap) {
  if (NULL == heap || 0 == heap->used) {
    return NULL;
  }

  if (heap->used == 1) {
    return heap->elements[1].value;
  }

  if (heap->used == 2) {
    return heap->elements[2].value;
  }

  if (compare(heap, 2, 3) > 0) {
    return heap->elements[2].value;
  } else {
    return heap->elements[3].value;
  }
}

void nr_minmax_heap_iterate(const nr_minmax_heap_t* heap,
                            nr_minmax_heap_iter_t callback,
                            void* userdata) {
  ssize_t i;

  if (NULL == heap || NULL == callback) {
    return;
  }

  for (i = 0; i <= heap->used; i++) {
    if (heap->elements[i].used) {
      (callback)(heap->elements[i].value, userdata);
    }
  }
}

void nr_minmax_heap_set_destructor(nr_minmax_heap_t* heap NRUNUSED,
                                   nr_minmax_heap_dtor_t destructor,
                                   void* destructor_userdata) {
  if (NULL == heap) {
    return;
  }

  heap->destructor = destructor;
  heap->destructor_userdata = destructor_userdata;
}
