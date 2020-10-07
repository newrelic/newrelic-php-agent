/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_stack.h"
#include "tlib_main.h"

static void test_bad_parameters(void) {
  tlib_pass_if_true("A NULL stack is empty", nr_stack_is_empty(NULL),
                    "Expected true");

  nr_stack_push(NULL, (void*)1);

  tlib_pass_if_null("A NULL stack pops NULL", nr_stack_pop(NULL));

  nr_stack_destroy_fields(NULL);
}

static void test_create_destroy(void) {
  nr_stack_t s;

  tlib_pass_if_true("A well-formed set of args must create a stack",
                    nr_stack_init(&s, 5), "Expected true");

  nr_stack_destroy_fields(&s);

  tlib_pass_if_int_equal("A destroyed stack should have 0 capacity", s.capacity,
                         0);
  tlib_pass_if_int_equal("A destroyed stack should have 0 size", s.used, 0);

  tlib_pass_if_false(
      "An ill-formed set of args cannot create a stack (no capacity)",
      nr_stack_init(&s, 0), "Expected false");
  tlib_pass_if_false(
      "An ill-formed set of args cannot create a stack (NULL ptr)",
      nr_stack_init(NULL, 5), "Expected false");
}

static void test_push_pop(void) {
  nr_stack_t s;

  nr_stack_init(&s, 3);

  tlib_pass_if_true("A newly-formed stack must be empty", nr_stack_is_empty(&s),
                    "Expected true");
  tlib_pass_if_not_null(
      "A newly-formed stack must have allocated memory for its elements",
      s.elements);
  tlib_pass_if_int_equal("A newly formed stack has a size of 0", s.used, 0);
  tlib_pass_if_int_equal("A newly formed stack must have the stated capacity",
                         s.capacity, 3);
  tlib_pass_if_null("Popping the top of an empty stack must yield NULL",
                    nr_stack_pop(&s));

  nr_stack_push(&s, (void*)1);
  nr_stack_push(&s, (void*)2);
  nr_stack_push(&s, (void*)3);

  tlib_pass_if_ptr_equal(
      "Popping the top of the stack must yield the most-recently pushed item "
      "(3)",
      nr_stack_pop(&s), (void*)3);
  tlib_pass_if_ptr_equal(
      "Popping the top of the stack must yield the most-recently pushed item "
      "(2)",
      nr_stack_pop(&s), (void*)2);
  tlib_pass_if_ptr_equal(
      "Popping the top of the stack must yield the most-recently pushed item "
      "(1)",
      nr_stack_pop(&s), (void*)1);

  tlib_pass_if_true("The stack must be empty", nr_stack_is_empty(&s),
                    "Expected true");

  nr_stack_destroy_fields(&s);
}

static void test_push_pop_extended(void) {
  nr_stack_t s;
  intptr_t i = 0;

  /* According to customer data research, the average depth of a trace is approx
   * 32 segments. */
  nr_stack_init(&s, 32);

  for (i = 1; i < 100; i++) {
    nr_stack_push(&s, (void*)i);
  }

  for (i = 99; i > 0; i--) {
    tlib_pass_if_ptr_equal(
        "Popping the top of the stack must yield the most-recently pushed "
        "item ",
        nr_stack_pop(&s), (void*)i);
  }

  nr_stack_destroy_fields(&s);
}

static void test_get(void) {
  nr_stack_t s;

  nr_stack_init(&s, 15);

  tlib_pass_if_null("Getting the top of an empty stack must yield NULL",
                    nr_stack_get_top(&s));

  nr_stack_push(&s, (void*)1);

  tlib_pass_if_ptr_equal(
      "Getting the top of a stack must yield the most recently pushed value",
      nr_stack_get_top(&s), (void*)1);
  tlib_pass_if_ptr_equal(
      "Getting the top of a stack must yield the most recently pushed value "
      "(again)",
      nr_stack_get_top(&s), (void*)1);

  nr_stack_destroy_fields(&s);
}

static void test_remove_topmost(void) {
  nr_stack_t s;

  nr_stack_init(&s, 15);

  tlib_pass_if_bool_equal(
      "Removing the topmost instance of an element on a NULL stack must fail",
      false, nr_stack_remove_topmost(NULL, NULL));

  tlib_pass_if_bool_equal(
      "Removing the topmost instance of an element on an empty stack must fail",
      false, nr_stack_remove_topmost(&s, NULL));

  nr_stack_push(&s, (void*)1);
  nr_stack_push(&s, (void*)2);
  nr_stack_push(&s, (void*)3);

  tlib_pass_if_bool_equal(
      "Removing the topmost instance of an element that does not exist must "
      "fail",
      false, nr_stack_remove_topmost(&s, (void*)4));

  tlib_pass_if_size_t_equal(
      "Removing the topmost instance of an element that does not exist must "
      "leave the stack in its original state",
      3, s.used);

  tlib_pass_if_bool_equal(
      "Removing the topmost instance of an extant element must succeed", true,
      nr_stack_remove_topmost(&s, (void*)2));

  tlib_pass_if_size_t_equal(
      "Removing the topmost instance of an extant element must actually remove "
      "it",
      2, s.used);

  tlib_pass_if_ptr_equal(
      "Removing the topmost instance of an extant element must not touch the "
      "other elements",
      (void*)1, nr_vector_get(&s, 0));

  tlib_pass_if_ptr_equal(
      "Removing the topmost instance of an extant element must not touch the "
      "other elements",
      (void*)3, nr_vector_get(&s, 1));

  nr_stack_destroy_fields(&s);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_create_destroy();
  test_push_pop();
  test_push_pop_extended();
  test_get();
  test_remove_topmost();
}
