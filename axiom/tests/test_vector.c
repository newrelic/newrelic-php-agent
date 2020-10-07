/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_vector.h"
#include "util_vector_private.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

#include <stdarg.h>

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void add_elements(nr_vector_t* v, size_t n) {
  uintptr_t i;
  size_t pre_add_size = v->used;

  for (i = 0; i < n; i++) {
    nr_vector_push_back(v, (void*)i);
  }

  tlib_pass_if_size_t_equal("adding elements changes the size",
                            pre_add_size + n, v->used);
  tlib_pass_if_bool_equal("adding elements changes the capacity", true,
                          v->capacity >= v->used);
}

typedef struct {
  size_t free_count;
} free_metadata_t;

static void free_wrapper(void* element, void* userdata NRUNUSED) {
  nr_free(element);

  if (userdata) {
    free_metadata_t* metadata = (free_metadata_t*)userdata;

    metadata->free_count += 1;
  }
}

static void pass_if_vector_equals(const nr_vector_t* v, size_t size, ...) {
  va_list args;
  size_t i;

  va_start(args, size);

  tlib_pass_if_size_t_equal("vector has expected size", size, v->used);
  for (i = 0; i < size; i++) {
    void* expected = va_arg(args, void*);
    char* msg = nr_formatf("vector has expected value at index %zu", i);

    tlib_pass_if_ptr_equal(msg, expected, v->elements[i]);
    nr_free(msg);
  }

  va_end(args);
}

static void test_create_destroy(void) {
  free_metadata_t free_metadata = {.free_count = 0};
  nr_vector_t* v = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal(
      "a NULL pointer should fail gracefully when destroyed", false,
      nr_vector_destroy(NULL));
  tlib_pass_if_bool_equal(
      "a pointer to a NULL vector should fail gracefully when destroyed", false,
      nr_vector_destroy(&v));

  /*
   * Test : Create defaults.
   */
  v = nr_vector_create(0, NULL, NULL);
  tlib_pass_if_not_null("a vector was created", v);
  tlib_pass_if_size_t_equal("a vector with 0 initial capacity has 8", 8,
                            v->capacity);
  tlib_pass_if_size_t_equal("a new vector has 0 used elements", 0, v->used);
  tlib_pass_if_not_null("a vector has an initial set of elements", v->elements);
  tlib_pass_if_null("a vector with a NULL destructor is valid", v->dtor);
  tlib_pass_if_null("a vector with a NULL destructor userdata is valid",
                    v->dtor_userdata);
  nr_vector_destroy(&v);

  /*
   * Test : Explicit destructor and capacity.
   */
  v = nr_vector_create(10, free_wrapper, &free_metadata);
  tlib_pass_if_not_null("a vector was created", v);
  tlib_pass_if_size_t_equal(
      "a vector with an initial capacity gets that capacity", 10, v->capacity);
  tlib_pass_if_size_t_equal("a new vector has 0 used elements", 0, v->used);
  tlib_pass_if_not_null("a vector has an initial set of elements", v->elements);
  tlib_pass_if_ptr_equal("a vector with a destructor is valid", free_wrapper,
                         v->dtor);
  tlib_pass_if_ptr_equal("a vector with a destructor userdata is valid",
                         &free_metadata, v->dtor_userdata);
  nr_vector_push_back(v, nr_malloc(sizeof(int)));
  nr_vector_destroy(&v);
  tlib_pass_if_size_t_equal(
      "a vector destroys its elements when a destructor is provided", 1,
      free_metadata.free_count);
}

static void test_init_deinit(void) {
  free_metadata_t free_metadata = {.free_count = 0};
  nr_vector_t v;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("a NULL vector cannot be initialised", false,
                          nr_vector_init(NULL, 0, NULL, NULL));
  tlib_pass_if_bool_equal("a NULL vector cannot be deinitialised", false,
                          nr_vector_deinit(NULL));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_bool_equal("a non-NULL vector can be initialised", true,
                          nr_vector_init(&v, 10, free_wrapper, &free_metadata));
  tlib_pass_if_size_t_equal(
      "a vector with an initial capacity gets that capacity", 10, v.capacity);
  tlib_pass_if_size_t_equal("a new vector has 0 used elements", 0, v.used);
  tlib_pass_if_not_null("a vector has an initial set of elements", v.elements);
  tlib_pass_if_ptr_equal("a vector with a destructor is valid", free_wrapper,
                         v.dtor);
  tlib_pass_if_ptr_equal("a vector with a destructor userdata is valid",
                         &free_metadata, v.dtor_userdata);
  nr_vector_push_back(&v, nr_malloc(sizeof(int)));
  nr_vector_deinit(&v);
  tlib_pass_if_size_t_equal(
      "a vector destroys its elements when a destructor is provided", 1,
      free_metadata.free_count);
  tlib_pass_if_size_t_equal("a finalised vector has 0 capacity", 0, v.capacity);
  tlib_pass_if_size_t_equal("a finalised vector has 0 size", 0, v.used);
}

static void test_getters(void) {
  nr_vector_t v;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_size_t_equal("a NULL vector has 0 capacity", 0,
                            nr_vector_capacity(NULL));
  tlib_pass_if_size_t_equal("a NULL vector has 0 size", 0,
                            nr_vector_size(NULL));

  /*
   * Test : Normal operation.
   */
  nr_vector_init(&v, 4, NULL, NULL);

  tlib_pass_if_size_t_equal("a new vector has its initial capacity", 4,
                            nr_vector_capacity(&v));
  tlib_pass_if_size_t_equal("a new vector has 0 size", 0, nr_vector_size(&v));

  nr_vector_push_back(&v, (void*)42);
  tlib_pass_if_size_t_equal(
      "pushing an item onto a vector results in its size being 1", 1,
      nr_vector_size(&v));

  nr_vector_deinit(&v);
}

static void test_ensure(void) {
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("a NULL vector cannot be ensured", false,
                          nr_vector_ensure(NULL, 8));
  tlib_pass_if_bool_equal("a vector cannot have a capacity of 0 ensured", false,
                          nr_vector_ensure(&v, 0));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_bool_equal("a vector can be ensured to a smaller capacity", true,
                          nr_vector_ensure(&v, 4));
  tlib_pass_if_size_t_equal(
      "a vector ensured to a smaller capacity retains its original capacity", 8,
      v.capacity);

  tlib_pass_if_bool_equal("a vector can be ensured to the same capacity", true,
                          nr_vector_ensure(&v, 8));
  tlib_pass_if_size_t_equal(
      "a vector ensured to the same capacity retains its original capacity", 8,
      v.capacity);

  tlib_pass_if_bool_equal(
      "a vector can be ensured to a slightly higher capacity", true,
      nr_vector_ensure(&v, 9));
  tlib_pass_if_size_t_equal("vectors grow by doubling in capacity", 16,
                            v.capacity);

  nr_vector_deinit(&v);
}

static void test_shrink_if_necessary(void) {
  nr_vector_t v;

  nr_vector_init(&v, 10, NULL, NULL);

  tlib_pass_if_bool_equal(
      "a vector with fewer than 4 elements will not be shrunk, successfully",
      true, nr_vector_shrink_if_necessary(&v));
  tlib_pass_if_size_t_equal(
      "a vector with fewer than 4 elements will not be shrunk", 10, v.capacity);

  add_elements(&v, 4);

  tlib_pass_if_bool_equal(
      "a vector with more than 4 elements will be shrunk if the capacity is "
      "more than double the number of elements",
      true, nr_vector_shrink_if_necessary(&v));
  tlib_pass_if_size_t_equal(
      "a vector with more than 4 elements will be shrunk if the capacity is "
      "more than double the number of elements",
      5, v.capacity);

  tlib_pass_if_bool_equal(
      "a vector with more than 4 elements will not be shrunk if the capacity "
      "is less than double the number of elements",
      true, nr_vector_shrink_if_necessary(&v));
  tlib_pass_if_size_t_equal(
      "a vector with more than 4 elements will not be shrunk if the capacity "
      "is less than double the number of elements",
      5, v.capacity);

  nr_vector_deinit(&v);
}

static void test_push(void) {
  uintptr_t i;
  nr_vector_t v;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("a NULL vector cannot be pushed to", false,
                          nr_vector_push_back(NULL, (void*)42));
  tlib_pass_if_bool_equal("a NULL vector cannot be pushed to", false,
                          nr_vector_push_front(NULL, (void*)42));

  /*
   * Test : Normal operation.
   */
  nr_vector_init(&v, 8, NULL, NULL);
  add_elements(&v, 6);

  tlib_pass_if_bool_equal("a vector can have an element pushed to the back",
                          true, nr_vector_push_back(&v, (void*)42));
  tlib_pass_if_size_t_equal("the vector has the expected size", 7, v.used);
  tlib_pass_if_ptr_equal("the new element is at the back", (void*)42,
                         v.elements[v.used - 1]);

  tlib_pass_if_bool_equal("a vector can have an element pushed to the front",
                          true, nr_vector_push_front(&v, (void*)43));
  tlib_pass_if_size_t_equal("the vector has the expected size", 8, v.used);
  tlib_pass_if_ptr_equal("the new element is at the front", (void*)43,
                         v.elements[0]);

  /*
   * Test : General expansion over time.
   */
  for (i = 0; i < 128; i++) {
    tlib_pass_if_bool_equal("a vector can have an element pushed to the front",
                            true, nr_vector_push_front(&v, (void*)(i + 1000)));
    tlib_pass_if_bool_equal("a vector can have an element pushed to the back",
                            true, nr_vector_push_back(&v, (void*)(i + 2000)));
  }

  tlib_pass_if_bool_equal("the vector has a capacity greater than 256", 512,
                          v.capacity);
  tlib_pass_if_bool_equal("the vector has the expected size", 128 * 2 + 8,
                          v.used);

  for (i = 0; i < 128; i++) {
    uintptr_t expected = (127 - i) + 1000;
    char* msg = nr_formatf("element %" PRIuPTR " is %" PRIuPTR, i, expected);

    tlib_pass_if_ptr_equal(msg, (void*)expected, v.elements[i]);

    nr_free(msg);
  }

  for (i = 0; i < 128; i++) {
    uintptr_t expected = (127 - i) + 2000;
    uintptr_t index = v.used - i - 1;
    char* msg
        = nr_formatf("element %" PRIuPTR " is %" PRIuPTR, index, expected);

    tlib_pass_if_ptr_equal(msg, (void*)expected, v.elements[index]);

    nr_free(msg);
  }

  nr_vector_deinit(&v);
}

static void test_pop(void) {
  uintptr_t i;
  nr_vector_t v;
  void* element = (void*)0xC0FFEE;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("a NULL vector cannot be popped from", false,
                          nr_vector_pop_back(NULL, &element));
  tlib_pass_if_ptr_equal("a failed pop doesn't change the receiving element",
                         (void*)0xC0FFEE, element);
  tlib_pass_if_bool_equal("a NULL vector cannot be popped from", false,
                          nr_vector_pop_front(NULL, &element));
  tlib_pass_if_ptr_equal("a failed pop doesn't change the receiving element",
                         (void*)0xC0FFEE, element);

  /*
   * Test : Normal operation.
   */
  nr_vector_init(&v, 8, NULL, NULL);

  add_elements(&v, 1000);
  for (i = 0; i < 1000; i++) {
    tlib_pass_if_bool_equal("popping from the front succeeds", true,
                            nr_vector_pop_front(&v, &element));
    tlib_pass_if_ptr_equal("popping from the front returns the expected value",
                           (void*)i, element);
    tlib_pass_if_size_t_equal("popping decreases the number of elements",
                              999 - i, v.used);
  }
  tlib_pass_if_size_t_equal("popping shrinks the vector", 8, v.capacity);

  add_elements(&v, 1000);
  for (i = 0; i < 1000; i++) {
    tlib_pass_if_bool_equal("popping from the back succeeds", true,
                            nr_vector_pop_back(&v, &element));
    tlib_pass_if_ptr_equal("popping from the back returns the expected value",
                           (void*)(999 - i), element);
    tlib_pass_if_size_t_equal("popping decreases the number of elements",
                              999 - i, v.used);
  }
  tlib_pass_if_size_t_equal("popping shrinks the vector", 8, v.capacity);

  nr_vector_deinit(&v);
}

static void test_insert(void) {
  nr_vector_t v;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("inserting to a NULL vector fails", false,
                          nr_vector_insert(NULL, 0, NULL));

  /*
   * Test : Normal operation.
   */
  nr_vector_init(&v, 8, NULL, NULL);

  tlib_pass_if_bool_equal("inserting to an empty vector succeeds", true,
                          nr_vector_insert(&v, 0, (void*)1));
  pass_if_vector_equals(&v, 1, (void*)1);

  tlib_pass_if_bool_equal(
      "inserting at position 0 is equivalent to pushing at the front", true,
      nr_vector_insert(&v, 0, (void*)2));
  pass_if_vector_equals(&v, 2, (void*)2, (void*)1);

  tlib_pass_if_bool_equal(
      "inserting at a position equal to the size is equivalent to pushing at "
      "the back",
      true, nr_vector_insert(&v, v.used, (void*)3));
  pass_if_vector_equals(&v, 3, (void*)2, (void*)1, (void*)3);

  tlib_pass_if_bool_equal(
      "inserting at a position greater than the size is equivalent to pushing "
      "at the back",
      true, nr_vector_insert(&v, v.used * 2, (void*)4));
  pass_if_vector_equals(&v, 4, (void*)2, (void*)1, (void*)3, (void*)4);

  tlib_pass_if_bool_equal("inserting at position 1 should move other elements",
                          true, nr_vector_insert(&v, 1, (void*)5));
  pass_if_vector_equals(&v, 5, (void*)2, (void*)5, (void*)1, (void*)3,
                        (void*)4);

  nr_vector_deinit(&v);
}

static void test_remove(void) {
  void* element = (void*)0xC0FFEE;
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("removing from a NULL vector fails", false,
                          nr_vector_remove(NULL, 0, &element));
  tlib_pass_if_ptr_equal("a failed removal does not change the element",
                         (void*)0xC0FFEE, element);

  tlib_pass_if_bool_equal("removing with a NULL element fails", false,
                          nr_vector_remove(&v, 0, NULL));

  tlib_pass_if_bool_equal("removing from an empty vector fails", false,
                          nr_vector_remove(&v, 0, &element));
  tlib_pass_if_ptr_equal("a failed removal does not change the element",
                         (void*)0xC0FFEE, element);

  /*
   * Test : Normal operation.
   */
  add_elements(&v, 8);

  tlib_pass_if_bool_equal(
      "removing the first element is equivalent to popping from the front",
      true, nr_vector_remove(&v, 0, &element));
  tlib_pass_if_ptr_equal("the element returned is correct", (void*)0, element);
  pass_if_vector_equals(&v, 7, (void*)1, (void*)2, (void*)3, (void*)4, (void*)5,
                        (void*)6, (void*)7);
  tlib_pass_if_size_t_equal("removing an element reduces the size", 7, v.used);

  tlib_pass_if_bool_equal(
      "removing the last element is equivalent to popping from the back", true,
      nr_vector_remove(&v, 6, &element));
  tlib_pass_if_ptr_equal("the element returned is correct", (void*)7, element);
  pass_if_vector_equals(&v, 6, (void*)1, (void*)2, (void*)3, (void*)4, (void*)5,
                        (void*)6);
  tlib_pass_if_size_t_equal("removing an element reduces the size", 6, v.used);

  tlib_pass_if_bool_equal(
      "removing the second element should move the other elements", true,
      nr_vector_remove(&v, 1, &element));
  tlib_pass_if_ptr_equal("the element returned is correct", (void*)2, element);
  pass_if_vector_equals(&v, 5, (void*)1, (void*)3, (void*)4, (void*)5,
                        (void*)6);
  tlib_pass_if_size_t_equal("removing an element reduces the size", 5, v.used);

  nr_vector_deinit(&v);
}

static void test_get(void) {
  uintptr_t i;
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("getting from a NULL vector fails", nr_vector_get(NULL, 0));

  tlib_pass_if_null("getting from an empty vector fails", nr_vector_get(&v, 0));

  /*
   * Test : Normal operation.
   */
  add_elements(&v, 8);

  for (i = 0; i < 8; i++) {
    tlib_pass_if_ptr_equal("getting a valid element succeeds", (void*)i,
                           nr_vector_get(&v, (size_t)i));
  }

  tlib_pass_if_null("access beyond the end of a vector fails",
                    nr_vector_get(&v, 8));

  nr_vector_deinit(&v);
}

static void test_get_element(void) {
  void* element = (void*)0xC0FFEE;
  uintptr_t i;
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("getting from a NULL vector fails", false,
                          nr_vector_get_element(NULL, 0, &element));
  tlib_pass_if_ptr_equal("a failed get does not change the element",
                         (void*)0xC0FFEE, element);

  tlib_pass_if_bool_equal("getting with a NULL element fails", false,
                          nr_vector_get_element(&v, 0, NULL));

  tlib_pass_if_bool_equal("getting from an empty vector fails", false,
                          nr_vector_get_element(&v, 0, &element));
  tlib_pass_if_ptr_equal("a failed get does not change the element",
                         (void*)0xC0FFEE, element);

  /*
   * Test : Normal operation.
   */
  add_elements(&v, 8);

  for (i = 0; i < 8; i++) {
    tlib_pass_if_bool_equal("getting a valid element succeeds", true,
                            nr_vector_get_element(&v, (size_t)i, &element));
    tlib_pass_if_ptr_equal("the element value is correct", (void*)i, element);
  }

  nr_vector_deinit(&v);
}

static void test_replace(void) {
  uintptr_t i;
  free_metadata_t free_metadata = {.free_count = 0};
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("replacing within a NULL vector fails", false,
                          nr_vector_replace(NULL, 0, (void*)42));

  tlib_pass_if_bool_equal("replacing within an empty vector fails", false,
                          nr_vector_replace(&v, 0, (void*)42));

  /*
   * Test : Normal operation.
   */
  add_elements(&v, 8);

  for (i = 0; i < 8; i++) {
    tlib_pass_if_bool_equal(
        "replacing a valid element without a destructor succeeds", true,
        nr_vector_replace(&v, (size_t)i, (void*)(i * i)));
  }

  pass_if_vector_equals(&v, 8, (void*)0, (void*)1, (void*)4, (void*)9,
                        (void*)16, (void*)25, (void*)36, (void*)49);

  nr_vector_deinit(&v);

  nr_vector_init(&v, 8, free_wrapper, &free_metadata);
  for (i = 0; i < 8; i++) {
    int* value = nr_malloc(sizeof(int));

    *value = (int)i;
    nr_vector_push_back(&v, value);
  }

  for (i = 0; i < 8; i++) {
    int* value = nr_malloc(sizeof(int));

    *value = (int)i;
    nr_vector_replace(&v, (size_t)i, value);
  }

  tlib_pass_if_size_t_equal(
      "replacing all values in a vector still results in the vector having the "
      "same size",
      8, v.used);
  tlib_pass_if_size_t_equal(
      "replacing all values in a vector resulted in the original values being "
      "freed",
      8, free_metadata.free_count);

  nr_vector_deinit(&v);
}

static intptr_t expected_sort_userdata;

static int uintptr_cmp(const void* a, const void* b, void* userdata) {
  tlib_pass_if_intptr_t_equal("userdata should match the expected userdata",
                              expected_sort_userdata, (intptr_t)userdata);

  return ((intptr_t)a) - ((intptr_t)b);
}

static void test_sort(void) {
  intptr_t i;
  const intptr_t num_elements = 100;
  nr_vector_t v;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("sorting a NULL vector should fail", false,
                          nr_vector_sort(&v, NULL, NULL));
  tlib_pass_if_bool_equal("sorting a vector with a NULL comparator should fail",
                          false, nr_vector_sort(NULL, uintptr_cmp, NULL));

  /*
   * Test : Normal operation.
   */
  nr_vector_init(&v, 8, NULL, NULL);

  // Insert a set of out of order numbers.
  for (i = 0; i < num_elements; i++) {
    nr_vector_push_back(&v, (void*)(num_elements - i - 1));
  }

  // Now sort.
  expected_sort_userdata = 42;
  tlib_pass_if_bool_equal("sorting a vector should succeed", true,
                          nr_vector_sort(&v, uintptr_cmp, (void*)42));

  // Now test.
  for (i = 0; i < num_elements; i++) {
    void* value = nr_vector_get(&v, i);

    tlib_pass_if_intptr_t_equal("expected value should match", i,
                                (intptr_t)value);
  }

  nr_vector_deinit(&v);
}

typedef struct {
  uintptr_t calls;
  uintptr_t limit;
} early_return_iterator_t;

static bool early_return_iterator_callback(void* element NRUNUSED,
                                           early_return_iterator_t* metadata) {
  return ++metadata->calls < metadata->limit;
}

static bool iterator_callback(void* element, uintptr_t* expected) {
  char* msg
      = nr_formatf("element %" PRIuPTR " has the expected value", *expected);

  tlib_pass_if_ptr_equal(msg, (void*)*expected, element);
  (*expected) += 1;

  nr_free(msg);
  return true;
}

static void test_iterate(void) {
  early_return_iterator_t erit = {.calls = 0, .limit = 4};
  uintptr_t expected = 0;
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal(
      "iterating over a NULL vector fails", false,
      nr_vector_iterate(NULL, (nr_vector_iter_t)iterator_callback, &expected));

  tlib_pass_if_bool_equal("iterating with a NULL callback fails", false,
                          nr_vector_iterate(&v, NULL, &expected));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_bool_equal(
      "iterating over an empty vector succeeds", true,
      nr_vector_iterate(&v, (nr_vector_iter_t)iterator_callback, &expected));
  tlib_pass_if_uintptr_t_equal(
      "iterating over an empty vector resulted in the expected number of "
      "callback invocations",
      0, expected);

  add_elements(&v, 8);

  tlib_pass_if_bool_equal(
      "iterating over a vector succeeds", true,
      nr_vector_iterate(&v, (nr_vector_iter_t)iterator_callback, &expected));
  tlib_pass_if_uintptr_t_equal(
      "iterating over a vector resulted in the expected number of callback "
      "invocations",
      v.used, expected);

  tlib_pass_if_bool_equal(
      "early return from iterating over a vector results in false being "
      "returned",
      false,
      nr_vector_iterate(&v, (nr_vector_iter_t)early_return_iterator_callback,
                        &erit));
  tlib_pass_if_uintptr_t_equal(
      "iterating over a vector resulted in the expected number of callback "
      "invocations",
      erit.limit, erit.calls);

  nr_vector_deinit(&v);
}

static void test_find(void) {
  size_t index = 0;
  void* userdata = (void*)12345;
  nr_vector_t v;

  nr_vector_init(&v, 8, NULL, NULL);
  expected_sort_userdata = (intptr_t)userdata;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("finding in a NULL vector returns false", false,
                          nr_vector_find_first(NULL, NULL, NULL, NULL, NULL));
  tlib_pass_if_bool_equal("finding in a NULL vector returns false", false,
                          nr_vector_find_last(NULL, NULL, NULL, NULL, NULL));

  /*
   * Test : Empty vector.
   */
  tlib_pass_if_bool_equal("finding in an empty vector returns false", false,
                          nr_vector_find_first(&v, NULL, NULL, NULL, NULL));
  tlib_pass_if_bool_equal("finding in an empty vector returns false", false,
                          nr_vector_find_last(&v, NULL, NULL, NULL, NULL));

  add_elements(&v, 8);

  /*
   * Test : Vector with the default comparator.
   */
  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with the default "
      "comparator and sets the index",
      true, nr_vector_find_first(&v, (void*)4, NULL, NULL, &index));
  tlib_pass_if_size_t_equal("finding a value within a vector sets the index", 4,
                            index);

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with the default "
      "comparator and sets the index",
      true, nr_vector_find_last(&v, (void*)5, NULL, NULL, &index));
  tlib_pass_if_size_t_equal("finding a value within a vector sets the index", 5,
                            index);

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with the default "
      "comparator and does not set the index if NULL is given",
      true, nr_vector_find_first(&v, (void*)4, NULL, NULL, NULL));

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with the default "
      "comparator and does not set the index if NULL is given",
      true, nr_vector_find_last(&v, (void*)5, NULL, NULL, NULL));

  index = 42;

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns false with the default "
      "comparator and does not set the index",
      false, nr_vector_find_first(&v, (void*)10, NULL, NULL, &index));
  tlib_pass_if_size_t_equal(
      "finding a value within a vector does not change the index if the value "
      "is not found",
      42, index);

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns false with the default "
      "comparator and does not set the index",
      false, nr_vector_find_last(&v, (void*)10, NULL, NULL, &index));
  tlib_pass_if_size_t_equal(
      "finding a value within a vector does not change the index if the value "
      "is not found",
      42, index);

  /*
   * Test : Vector with a custom comparator.
   */
  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with a custom comparator "
      "and sets the index",
      true, nr_vector_find_first(&v, (void*)4, uintptr_cmp, userdata, &index));
  tlib_pass_if_size_t_equal("finding a value within a vector sets the index", 4,
                            index);

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with a custom comparator "
      "comparator and sets the index",
      true, nr_vector_find_last(&v, (void*)5, uintptr_cmp, userdata, &index));
  tlib_pass_if_size_t_equal("finding a value within a vector sets the index", 5,
                            index);

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with a custom comparator "
      "comparator and does not set the index if NULL is given",
      true, nr_vector_find_first(&v, (void*)4, uintptr_cmp, userdata, NULL));

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns true with a custom comparator "
      "comparator and does not set the index if NULL is given",
      true, nr_vector_find_last(&v, (void*)5, uintptr_cmp, userdata, NULL));

  index = 42;

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns false with a custom comparator "
      "comparator and does not set the index",
      false,
      nr_vector_find_first(&v, (void*)10, uintptr_cmp, userdata, &index));
  tlib_pass_if_size_t_equal(
      "finding a value within a vector does not change the index if the value "
      "is not found",
      42, index);

  tlib_pass_if_bool_equal(
      "finding a value within a vector returns false with a custom comparator "
      "comparator and does not set the index",
      false, nr_vector_find_last(&v, (void*)10, uintptr_cmp, userdata, &index));
  tlib_pass_if_size_t_equal(
      "finding a value within a vector does not change the index if the value "
      "is not found",
      42, index);

  nr_vector_deinit(&v);
}

void test_main(void* p NRUNUSED) {
  test_create_destroy();
  test_init_deinit();
  test_getters();
  test_ensure();
  test_shrink_if_necessary();
  test_push();
  test_pop();
  test_insert();
  test_remove();
  test_get();
  test_get_element();
  test_replace();
  test_sort();
  test_iterate();
  test_find();
}
