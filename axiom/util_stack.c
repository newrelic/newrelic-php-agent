/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "util_stack.h"

bool nr_stack_init(nr_stack_t* s, size_t capacity) {
  if (0 == capacity) {
    return false;
  }

  return nr_vector_init(s, capacity, NULL, NULL);
}

void* nr_stack_get_top(nr_stack_t* s) {
  size_t size = nr_vector_size(s);

  if (nrunlikely(0 == size)) {
    return NULL;
  }

  return nr_vector_get(s, size - 1);
}

void nr_stack_push(nr_stack_t* s, void* new_element) {
  nr_vector_push_back(s, new_element);
}

void* nr_stack_pop(nr_stack_t* s) {
  void* element = NULL;

  nr_vector_pop_back(s, &element);
  return element;
}

void nr_stack_destroy_fields(nr_stack_t* s) {
  nr_vector_deinit(s);
}

bool nr_stack_remove_topmost(nr_stack_t* s, const void* element) {
  size_t index = 0;

  // The stack pushes to the end of the vector, so we need to search in reverse
  // order.
  if (nr_vector_find_last(s, element, NULL, NULL, &index)) {
    void* found = NULL;

    // nr_vector_remove() requires a pointer to receive the element, but since
    // stacks don't take ownership of the elements within them, we can let it
    // leak and rely on the code that owns the element to clean it up later.
    return nr_vector_remove(s, index, &found);
  }

  return false;
}

nr_stack_t nr_stack_copy(nr_stack_t* src, nr_stack_clone_elem_t clone) {
  nr_stack_t dest;
  size_t n;

  if (NULL == src || NULL == clone) {
    nr_vector_init(&dest, NR_STACK_DEFAULT_CAPACITY, NULL, NULL);
    return dest;
  }

  n = nr_vector_size(src);
  nr_vector_init(&dest, n ? n : NR_STACK_DEFAULT_CAPACITY, src->dtor, NULL);

  for (size_t i = 0; i < n; i++) {
    void* orig = nr_vector_get(src, i);
    nr_stack_push(&dest, clone(orig));
  }

  return dest;
}
