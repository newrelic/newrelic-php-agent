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

#endif /* NR_STACK_HDR */
