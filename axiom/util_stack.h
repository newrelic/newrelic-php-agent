/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains data types and functions for dealing with a stack.
 *
 * This file is agent-agnostic. It defines a simple stack data type and
 * functions used to push, pop, and get the top element from the stack.
 */
#ifndef NR_STACK_HDR
#define NR_STACK_HDR

#include <stdbool.h>

#include "util_memory.h"
#include "util_vector.h"

#define NR_STACK_DEFAULT_CAPACITY 32

typedef struct _nr_vector_t nr_stack_t;
typedef void* (*nr_stack_clone_elem_ptr_t)(void* element);

/*
 * Purpose : Initialize a stack data type.
 *
 * Params  : 1. A pointer to a stack, s.
 *           2. Initial capacity for the stack; the stack capacity doubles when
 *              full.
 *
 * Returns : true if successful, false otherwise.
 */
bool nr_stack_init(nr_stack_t* s, size_t capacity);

/*
 * Purpose : Determine whether a stack is empty.
 *
 * Params  : 1. A pointer to a stack, s.
 *
 * Returns : true if empty, false otherwise.
 *
 */
static inline bool nr_stack_is_empty(nr_stack_t* s) {
  if (s != NULL) {
    return 0 == nr_vector_size(s);
  }

  return true;
}

/*
 * Purpose : Peek at the top of the stack without removing the top element.
 *
 * Params  : 1. A pointer to a stack, s.
 *
 * Returns : A pointer to the element; NULL if the stack is empty.
 *
 */
void* nr_stack_get_top(nr_stack_t* s);

/*
 * Purpose : Push a new element onto the top of the stack.
 *
 * Params  : 1. A pointer to a stack, s.
 *           2.
 *
 * Returns : A pointer to the element; NULL if the stack is empty.
 *
 */
void nr_stack_push(nr_stack_t* s, void* new_element);

/*
 * Purpose : Remove and return the top of the stack
 *
 * Params  : 1. A pointer to a stack, s.
 *
 * Returns : A pointer to the element; NULL if the stack is empty.
 *
 */
void* nr_stack_pop(nr_stack_t* s);

/*
 * Purpose : Free the dynamically-allocated memory for a stack.
 *
 * Params  : 1. A pointer to a stack, s.
 *
 */
void nr_stack_destroy_fields(nr_stack_t* s);

/*
 * Purpose : Remove the topmost instance of an element in a stack.
 *
 * Params  : 1. A pointer to a stack, s.
 *           2. The element to find and remove.
 *
 * Returns : True if the element was found and removed; false otherwise or on
 *           error.
 */
bool nr_stack_remove_topmost(nr_stack_t* s, const void* element);

/*
 * Purpose : Create a copy of a stack. Each element of src is passed through
 *           the supplied clone callback to produce the corresponding element
 *           in the returned stack, so the depth of the copy is determined by
 *           the callback (return the input pointer for a shallow copy, or
 *           allocate and populate a duplicate for a deep copy). The source
 *           stack's contents and order are preserved.
 *
 *           The destination stack inherits the source stack's destructor
 *           (src->dtor); its destructor userdata is set to NULL.
 *
 * Params  : 1. A pointer to the source stack, src.
 *           2. A clone callback invoked once per element.
 *
 * Returns : A new stack containing the cloned elements of src, in the same
 *           order. If src or clone is NULL, an initialized empty stack is
 *           returned. The caller is responsible for releasing the returned
 *           stack with nr_stack_destroy_fields().
 */
nr_stack_t nr_stack_copy(nr_stack_t* src, nr_stack_clone_elem_ptr_t clone);

#endif /* NR_STACK_HDR */
