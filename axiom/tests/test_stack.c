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

static void* clone_identity(void* element) {
  return element;
}

static int clone_call_count = 0;

static void* clone_counting(void* element) {
  clone_call_count++;
  return element;
}

static void* clone_strdup(void* element) {
  return nr_strdup((const char*)element);
}

static int dtor_call_count = 0;

static void dtor_free_string(void* element, void* userdata NRUNUSED) {
  dtor_call_count++;
  nr_free(element);
}

static void test_copy_empty(void) {
  nr_stack_t src;
  nr_stack_t dest;

  nr_stack_init(&src, 5);

  dest = nr_stack_copy(&src, clone_identity);

  tlib_pass_if_true("Copy of an empty stack must be empty",
                    nr_stack_is_empty(&dest), "Expected true");
  tlib_pass_if_size_t_equal("Copy of an empty stack must have size 0", 0,
                            dest.used);
  tlib_pass_if_not_null(
      "Copy of an empty stack must have allocated memory for its elements",
      dest.elements);

  nr_stack_destroy_fields(&src);
  nr_stack_destroy_fields(&dest);
}

static void test_copy_preserves_order(void) {
  nr_stack_t src;
  nr_stack_t dest;

  nr_stack_init(&src, 5);
  nr_stack_push(&src, (void*)1);
  nr_stack_push(&src, (void*)2);
  nr_stack_push(&src, (void*)3);

  dest = nr_stack_copy(&src, clone_identity);

  tlib_pass_if_size_t_equal("Copy must have the same size as the source", 3,
                            dest.used);
  tlib_pass_if_ptr_equal("Copy must preserve order: top element matches",
                         (void*)3, nr_stack_pop(&dest));
  tlib_pass_if_ptr_equal("Copy must preserve order: middle element matches",
                         (void*)2, nr_stack_pop(&dest));
  tlib_pass_if_ptr_equal("Copy must preserve order: bottom element matches",
                         (void*)1, nr_stack_pop(&dest));

  tlib_pass_if_size_t_equal("Source must remain unchanged after copy", 3,
                            src.used);
  tlib_pass_if_ptr_equal("Source top must remain unchanged after copy",
                         (void*)3, nr_stack_get_top(&src));

  nr_stack_destroy_fields(&src);
  nr_stack_destroy_fields(&dest);
}

static void test_copy_invokes_clone_per_element(void) {
  nr_stack_t src;
  nr_stack_t dest;

  clone_call_count = 0;

  nr_stack_init(&src, 5);
  nr_stack_push(&src, (void*)1);
  nr_stack_push(&src, (void*)2);
  nr_stack_push(&src, (void*)3);
  nr_stack_push(&src, (void*)4);

  dest = nr_stack_copy(&src, clone_counting);

  tlib_pass_if_int_equal("Clone callback must be invoked once per element", 4,
                         clone_call_count);

  nr_stack_destroy_fields(&src);
  nr_stack_destroy_fields(&dest);
}

static void test_copy_deep(void) {
  nr_stack_t src;
  nr_stack_t dest;
  char* a = nr_strdup("alpha");
  char* b = nr_strdup("beta");
  void* dest_top;

  /* Use nr_vector_init directly to install a destructor; the inherited
     dtor will free the cloned strings when dest is destroyed. */
  nr_vector_init(&src, 5, dtor_free_string, NULL);
  nr_stack_push(&src, a);
  nr_stack_push(&src, b);

  dtor_call_count = 0;
  dest = nr_stack_copy(&src, clone_strdup);

  tlib_pass_if_size_t_equal("Deep copy must have the same size as the source",
                            2, dest.used);

  dest_top = nr_stack_get_top(&dest);
  tlib_pass_if_str_equal("Deep copy contents must equal source contents",
                         "beta", (const char*)dest_top);
  tlib_pass_if_true(
      "Deep copy must produce distinct allocations from the source",
      dest_top != b, "Expected pointer inequality");

  nr_stack_destroy_fields(&dest);

  tlib_pass_if_int_equal(
      "Destroying the copy must invoke the inherited dtor on each cloned "
      "element",
      2, dtor_call_count);

  nr_stack_destroy_fields(&src);

  tlib_pass_if_int_equal(
      "Destroying the source must invoke the dtor on the source's own "
      "elements, leaving cloned-element frees independent",
      4, dtor_call_count);
}

static void test_copy_independent_of_source(void) {
  nr_stack_t src;
  nr_stack_t dest;

  nr_stack_init(&src, 5);
  nr_stack_push(&src, (void*)10);
  nr_stack_push(&src, (void*)20);

  dest = nr_stack_copy(&src, clone_identity);

  /* Mutating the source must not affect the copy. */
  nr_stack_push(&src, (void*)30);

  tlib_pass_if_size_t_equal(
      "Pushing to the source after copy must not affect the copy", 2,
      dest.used);
  tlib_pass_if_ptr_equal("Copy's top must remain the source's top at copy time",
                         (void*)20, nr_stack_get_top(&dest));

  /* Mutating the copy must not affect the source. */
  (void)nr_stack_pop(&dest);
  tlib_pass_if_size_t_equal(
      "Popping from the copy must not affect the source's size", 3, src.used);
  tlib_pass_if_ptr_equal("Source's top must remain unchanged after copy pop",
                         (void*)30, nr_stack_get_top(&src));

  nr_stack_destroy_fields(&src);
  nr_stack_destroy_fields(&dest);
}

static void test_copy_null_inputs(void) {
  nr_stack_t src;
  nr_stack_t dest;

  clone_call_count = 0;

  dest = nr_stack_copy(NULL, clone_counting);
  tlib_pass_if_true("Copy of a NULL source must be empty",
                    nr_stack_is_empty(&dest), "Expected true");
  tlib_pass_if_not_null(
      "Copy of a NULL source must be an initialized stack with allocated "
      "element storage",
      dest.elements);
  tlib_pass_if_int_equal(
      "Copy of a NULL source must not invoke the clone callback", 0,
      clone_call_count);
  nr_stack_destroy_fields(&dest);

  nr_stack_init(&src, 5);
  nr_stack_push(&src, (void*)1);
  nr_stack_push(&src, (void*)2);

  dest = nr_stack_copy(&src, NULL);
  tlib_pass_if_true(
      "Copy with a NULL clone callback must produce an empty stack",
      nr_stack_is_empty(&dest), "Expected true");
  tlib_pass_if_not_null(
      "Copy with a NULL clone callback must produce an initialized stack with "
      "allocated element storage",
      dest.elements);

  tlib_pass_if_size_t_equal(
      "Source must remain unchanged after a copy with a NULL clone", 2,
      src.used);

  nr_stack_destroy_fields(&dest);
  nr_stack_destroy_fields(&src);
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
  test_copy_empty();
  test_copy_preserves_order();
  test_copy_invokes_clone_per_element();
  test_copy_deep();
  test_copy_independent_of_source();
  test_copy_null_inputs();
}
