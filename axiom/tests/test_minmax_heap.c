/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_memory.h"
#include "util_minmax_heap.h"
#include "util_minmax_heap_private.h"

#include "tlib_main.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

/* A simple list type to affirm that heap iteration over values is correct. This
 * type is for test purposes only. */
#define NR_TEST_LIST_CAPACITY 10
typedef struct _nr_test_list_t {
  size_t capacity;
  int used;
  int32_t elements[NR_TEST_LIST_CAPACITY];
} nr_test_list_t;

typedef struct {
  int32_t value;
} test_t;

static test_t* test_new(int32_t value) {
  test_t* test = nr_malloc(sizeof(test_t));

  test->value = value;
  return test;
}

static void test_destroy(test_t** test_ptr) {
  nr_realfree((void**)test_ptr);
}

static int test_compare_impl(const test_t* a, const test_t* b) {
  if (a->value < b->value) {
    return -1;
  } else if (a->value > b->value) {
    return 1;
  }
  return 0;
}

static const uintptr_t compare_userdata = 1;
static int test_compare_with_userdata(const test_t* a,
                                      const test_t* b,
                                      void* userdata) {
  uintptr_t ud_value = (uintptr_t)userdata;

  tlib_pass_if_uintptr_t_equal("compare userdata", compare_userdata, ud_value);
  return test_compare_impl(a, b);
}

static int test_compare_without_userdata(const test_t* a,
                                         const test_t* b,
                                         void* userdata) {
  tlib_pass_if_null("compare userdata", userdata);
  return test_compare_impl(a, b);
}

static const uintptr_t destructor_userdata = 2;
static void test_destructor_with_userdata(test_t* test, void* userdata) {
  uintptr_t ud_value = (uintptr_t)userdata;

  tlib_pass_if_uintptr_t_equal("destructor userdata", destructor_userdata,
                               ud_value);
  test_destroy(&test);
}

static void test_destructor_without_userdata(test_t* test, void* userdata) {
  tlib_pass_if_null("destructor userdata", userdata);
  test_destroy(&test);
}

typedef struct {
  size_t calls;
} test_iterator_state;

static bool test_iterator_callback(const void* value NRUNUSED,
                                   test_iterator_state* state) {
  if (state) {
    state->calls++;
  }
  return true;
}

static bool test_value_iterator_callback(void* value, void* userdata) {
  nr_test_list_t* list;
  test_t* element;

  if (value && userdata) {
    list = (nr_test_list_t*)userdata;
    element = (test_t*)value;

    list->elements[list->used] = element->value;
    list->used = list->used + 1;
  }
  return true;
}

static void test_bad_parameters(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);

  /*
   * Test : Functions that return special values.
   */
  tlib_pass_if_ssize_t_equal("NULL heap: bound", 0, nr_minmax_heap_bound(NULL));
  tlib_pass_if_ssize_t_equal("NULL heap: capacity", 0,
                             nr_minmax_heap_capacity(NULL));
  tlib_pass_if_ssize_t_equal("NULL heap: size", 0, nr_minmax_heap_size(NULL));
  tlib_pass_if_null("NULL heap: pop_min", nr_minmax_heap_pop_min(NULL));
  tlib_pass_if_null("NULL heap: pop_max", nr_minmax_heap_pop_max(NULL));
  tlib_pass_if_null("NULL heap: peek_min", nr_minmax_heap_peek_min(NULL));
  tlib_pass_if_null("NULL heap: peek_max", nr_minmax_heap_peek_max(NULL));

  /*
   * Test : Functions that just shouldn't crash.
   */
  nr_minmax_heap_insert(NULL, NULL);
  nr_minmax_heap_iterate(NULL, NULL, NULL);
  nr_minmax_heap_iterate(heap, NULL, NULL);
  nr_minmax_heap_iterate(NULL, (nr_minmax_heap_iter_t)test_iterator_callback,
                         NULL);

  nr_minmax_heap_destroy(&heap);
}

static void test_create_destroy(void) {
  nr_minmax_heap_t* heap;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null(
      "NULL comparator",
      nr_minmax_heap_create(
          0, NULL, NULL,
          (nr_minmax_heap_dtor_t)test_destructor_without_userdata, NULL));

  tlib_pass_if_null(
      "invalid bound",
      nr_minmax_heap_create(
          -1, (nr_minmax_heap_cmp_t)test_compare_without_userdata, NULL,
          (nr_minmax_heap_dtor_t)test_destructor_without_userdata, NULL));

  tlib_pass_if_null(
      "invalid bound",
      nr_minmax_heap_create(
          1, (nr_minmax_heap_cmp_t)test_compare_without_userdata, NULL,
          (nr_minmax_heap_dtor_t)test_destructor_without_userdata, NULL));

  /*
   * Test : Normal operation.
   */
  heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);

  tlib_pass_if_not_null("unbounded, userdata", heap);
  tlib_pass_if_ssize_t_equal("unbounded bound", 0, heap->bound);
  tlib_pass_if_ssize_t_equal("unbounded capacity", NR_MINMAX_HEAP_CHUNK_SIZE,
                             heap->capacity);
  tlib_pass_if_ssize_t_equal("unbounded used", 0, heap->used);
  tlib_pass_if_not_null("unbounded elements", heap->elements);
  tlib_pass_if_ptr_equal("unbounded comparator", test_compare_with_userdata,
                         heap->comparator);
  tlib_pass_if_uintptr_t_equal("unbounded comparator userdata",
                               compare_userdata,
                               (uintptr_t)heap->comparator_userdata);
  tlib_pass_if_ptr_equal("unbounded destructor", test_destructor_with_userdata,
                         heap->destructor);
  tlib_pass_if_uintptr_t_equal("unbounded destructor userdata",
                               destructor_userdata,
                               (uintptr_t)heap->destructor_userdata);

  nr_minmax_heap_destroy(&heap);
  tlib_pass_if_null("destroy", heap);

  heap = nr_minmax_heap_create(
      10, (nr_minmax_heap_cmp_t)test_compare_with_userdata, NULL,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata, NULL);

  tlib_pass_if_not_null("bounded, no userdata", heap);
  tlib_pass_if_ssize_t_equal("bounded bound", 10, heap->bound);
  tlib_pass_if_ssize_t_equal("bounded capacity", 11, heap->capacity);
  tlib_pass_if_ssize_t_equal("bounded used", 0, heap->used);
  tlib_pass_if_not_null("bounded elements", heap->elements);
  tlib_pass_if_ptr_equal("bounded comparator", test_compare_with_userdata,
                         heap->comparator);
  tlib_pass_if_null("bounded comparator userdata", heap->comparator_userdata);
  tlib_pass_if_ptr_equal("bounded destructor", test_destructor_with_userdata,
                         heap->destructor);
  tlib_pass_if_null("bounded destructor userdata", heap->destructor_userdata);

  nr_minmax_heap_destroy(&heap);
  tlib_pass_if_null("destroy", heap);
}

static void test_empty(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);
  test_iterator_state state = {.calls = 0};

  tlib_pass_if_ssize_t_equal("bound", 0, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("size", 0, nr_minmax_heap_size(heap));
  tlib_pass_if_null("pop_min", nr_minmax_heap_pop_min(heap));
  tlib_pass_if_null("pop_max", nr_minmax_heap_pop_max(heap));
  tlib_pass_if_null("peek_min", nr_minmax_heap_peek_min(heap));
  tlib_pass_if_null("peek_max", nr_minmax_heap_peek_max(heap));

  nr_minmax_heap_iterate(heap, (nr_minmax_heap_iter_t)test_iterator_callback,
                         &state);
  tlib_pass_if_size_t_equal("iterator calls", 0, state.calls);

  nr_minmax_heap_destroy(&heap);
}

static void test_single_element(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);
  test_iterator_state state = {.calls = 0};
  test_t* test = test_new(42);

  nr_minmax_heap_insert(heap, test);

  tlib_pass_if_ssize_t_equal("bound", 0, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("size", 1, nr_minmax_heap_size(heap));
  tlib_pass_if_ptr_equal("peek_min", test, nr_minmax_heap_peek_min(heap));
  tlib_pass_if_ptr_equal("peek_max", test, nr_minmax_heap_peek_max(heap));
  tlib_pass_if_ptr_equal("pop_min", test, nr_minmax_heap_pop_min(heap));

  tlib_pass_if_ssize_t_equal("size", 0, nr_minmax_heap_size(heap));
  nr_minmax_heap_insert(heap, test);
  tlib_pass_if_ssize_t_equal("size", 1, nr_minmax_heap_size(heap));

  tlib_pass_if_ptr_equal("pop_max", test, nr_minmax_heap_pop_max(heap));

  tlib_pass_if_ssize_t_equal("size", 0, nr_minmax_heap_size(heap));
  nr_minmax_heap_insert(heap, test);
  tlib_pass_if_ssize_t_equal("size", 1, nr_minmax_heap_size(heap));

  nr_minmax_heap_iterate(heap, (nr_minmax_heap_iter_t)test_iterator_callback,
                         &state);
  tlib_pass_if_size_t_equal("iterator calls", 1, state.calls);

  nr_minmax_heap_destroy(&heap);
}

static void test_value_iteration(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);
  size_t i;
  int j;
  int32_t values[] = {5, 10, 15, 20, 25, 30, 35, 40};
  nr_test_list_t list = {.capacity = NR_TEST_LIST_CAPACITY, .used = 0};

  for (i = 0; i < sizeof(values) / sizeof(int32_t); i++) {
    nr_minmax_heap_insert(heap, test_new(values[i]));
  }

  nr_minmax_heap_iterate(
      heap, (nr_minmax_heap_iter_t)test_value_iterator_callback, &list);

  tlib_pass_if_int32_t_equal("list size", list.used, 8);

  /* Affirm that each value in the heap made it into the list */
  for (i = 0; i < sizeof(values) / sizeof(int32_t); i++) {
    bool found = false;

    for (j = 0; j < list.used; j++) {
      if (values[i] == list.elements[j]) {
        found = true;
      }
    }

    tlib_pass_if_true("list value", found, "Expected true");
  }

  nr_minmax_heap_destroy(&heap);
}

static void test_small(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);
  size_t i;
  test_iterator_state state = {.calls = 0};
  const test_t* test_peek;
  test_t* test_pop;
  int32_t values[] = {5, 10, 0, 60, 30, -20, 0, 15};

  for (i = 0; i < sizeof(values) / sizeof(int32_t); i++) {
    nr_minmax_heap_insert(heap, test_new(values[i]));
  }

  tlib_pass_if_ssize_t_equal("bound", 0, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("size", 8, nr_minmax_heap_size(heap));

  test_pop = (test_t*)nr_minmax_heap_pop_min(heap);
  tlib_pass_if_not_null("pop_min pointer", test_pop);
  tlib_pass_if_int32_t_equal("pop_min value", -20, test_pop->value);
  test_destroy(&test_pop);

  test_pop = (test_t*)nr_minmax_heap_pop_max(heap);
  tlib_pass_if_not_null("pop_max pointer", test_pop);
  tlib_pass_if_int32_t_equal("pop_max value", 60, test_pop->value);
  test_destroy(&test_pop);

  test_peek = (const test_t*)nr_minmax_heap_peek_min(heap);
  tlib_pass_if_not_null("peek_min pointer", test_peek);
  tlib_pass_if_int32_t_equal("peek_min value", 0, test_peek->value);

  test_peek = (const test_t*)nr_minmax_heap_peek_max(heap);
  tlib_pass_if_not_null("peek_max pointer", test_peek);
  tlib_pass_if_int32_t_equal("peek_max value", 30, test_peek->value);

  nr_minmax_heap_iterate(heap, (nr_minmax_heap_iter_t)test_iterator_callback,
                         &state);
  tlib_pass_if_size_t_equal("iterator calls", 6, state.calls);

  nr_minmax_heap_destroy(&heap);
}

static void test_expanded(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      0, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);
  size_t i;
  test_iterator_state state = {.calls = 0};
  const test_t* test_peek;
  test_t* test_pop;
  int32_t values[] = {5, 10, 0, 60, 30, -20, 0, 15};

  for (i = 0; i < 80; i++) {
    nr_minmax_heap_insert(
        heap, test_new(values[i % (sizeof(values) / sizeof(int32_t))]));
  }

  tlib_pass_if_ssize_t_equal("bound", 0, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("size", 80, nr_minmax_heap_size(heap));

  test_pop = (test_t*)nr_minmax_heap_pop_min(heap);
  tlib_pass_if_not_null("pop_min pointer", test_pop);
  tlib_pass_if_int32_t_equal("pop_min value", -20, test_pop->value);
  test_destroy(&test_pop);

  test_pop = (test_t*)nr_minmax_heap_pop_max(heap);
  tlib_pass_if_not_null("pop_max pointer", test_pop);
  tlib_pass_if_int32_t_equal("pop_max value", 60, test_pop->value);
  test_destroy(&test_pop);

  test_peek = (const test_t*)nr_minmax_heap_peek_min(heap);
  tlib_pass_if_not_null("peek_min pointer", test_peek);
  tlib_pass_if_int32_t_equal("peek_min value", -20, test_peek->value);

  test_peek = (const test_t*)nr_minmax_heap_peek_max(heap);
  tlib_pass_if_not_null("peek_max pointer", test_peek);
  tlib_pass_if_int32_t_equal("peek_max value", 60, test_peek->value);

  nr_minmax_heap_iterate(heap, (nr_minmax_heap_iter_t)test_iterator_callback,
                         &state);
  tlib_pass_if_size_t_equal("iterator calls", 78, state.calls);

  nr_minmax_heap_destroy(&heap);
}

static void test_bounded(void) {
  nr_minmax_heap_t* heap = nr_minmax_heap_create(
      4, (nr_minmax_heap_cmp_t)test_compare_with_userdata,
      (void*)compare_userdata,
      (nr_minmax_heap_dtor_t)test_destructor_with_userdata,
      (void*)destructor_userdata);
  size_t i;
  test_iterator_state state = {.calls = 0};
  const test_t* test_peek;
  test_t* test_pop;
  int32_t values[] = {5, 10, 0, 60, 30, -20, 0, 15};

  for (i = 0; i < sizeof(values) / sizeof(int32_t); i++) {
    nr_minmax_heap_insert(heap, test_new(values[i]));
  }

  tlib_pass_if_ssize_t_equal("bound", 4, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("capacity", 4, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("size", 4, nr_minmax_heap_size(heap));

  test_pop = (test_t*)nr_minmax_heap_pop_min(heap);
  tlib_pass_if_not_null("pop_min pointer", test_pop);
  tlib_pass_if_int32_t_equal("pop_min value", 10, test_pop->value);
  test_destroy(&test_pop);

  test_pop = (test_t*)nr_minmax_heap_pop_max(heap);
  tlib_pass_if_not_null("pop_max pointer", test_pop);
  tlib_pass_if_int32_t_equal("pop_max value", 60, test_pop->value);
  test_destroy(&test_pop);

  test_peek = (const test_t*)nr_minmax_heap_peek_min(heap);
  tlib_pass_if_not_null("peek_min pointer", test_peek);
  tlib_pass_if_int32_t_equal("peek_min value", 15, test_peek->value);

  test_peek = (const test_t*)nr_minmax_heap_peek_max(heap);
  tlib_pass_if_not_null("peek_max pointer", test_peek);
  tlib_pass_if_int32_t_equal("peek_max value", 30, test_peek->value);

  nr_minmax_heap_iterate(heap, (nr_minmax_heap_iter_t)test_iterator_callback,
                         &state);
  tlib_pass_if_size_t_equal("iterator calls", 2, state.calls);

  tlib_pass_if_ssize_t_equal("bound", 4, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("capacity", 4, nr_minmax_heap_bound(heap));
  tlib_pass_if_ssize_t_equal("size", 2, nr_minmax_heap_size(heap));

  nr_minmax_heap_destroy(&heap);
}

static void test_destructor_count_callback(void* value NRUNUSED,
                                           void* userdata) {
  size_t* count = (size_t*)userdata;

  test_destroy((test_t**)&value);

  (*count) += 1;
}

static void test_set_destructor(void) {
  nr_minmax_heap_t* heap;
  size_t destructor_called = 0;
  test_t* test;

  /*
   * Bad parameters, don't blow up.
   */
  nr_minmax_heap_set_destructor(NULL, NULL, NULL);

  /*
   * Test : Normal operation.
   */
  heap = nr_minmax_heap_create(
      2, (nr_minmax_heap_cmp_t)test_compare_without_userdata, NULL,
      (nr_minmax_heap_dtor_t)test_destructor_count_callback,
      (void*)&destructor_called);

  nr_minmax_heap_insert(heap, test_new(2));
  tlib_pass_if_size_t_equal("no destructor called when empty", 0,
                            destructor_called);

  nr_minmax_heap_insert(heap, test_new(3));
  tlib_pass_if_size_t_equal("no destructor called when size is 1", 0,
                            destructor_called);

  nr_minmax_heap_insert(heap, test_new(4));
  tlib_pass_if_size_t_equal("destructor called", 1, destructor_called);

  nr_minmax_heap_set_destructor(heap, NULL, NULL);

  test = test_new(1);
  nr_minmax_heap_insert(heap, test);
  tlib_pass_if_size_t_equal("no destructor called when unset", 1,
                            destructor_called);
  test_destroy(&test);

  nr_minmax_heap_set_destructor(
      heap, (nr_minmax_heap_dtor_t)test_destructor_count_callback,
      (void*)&destructor_called);

  nr_minmax_heap_insert(heap, test_new(6));
  tlib_pass_if_size_t_equal("destructor called when set by API", 2,
                            destructor_called);

  nr_minmax_heap_destroy(&heap);
}

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_create_destroy();
  test_empty();
  test_single_element();
  test_value_iteration();
  test_small();
  test_expanded();
  test_bounded();
  test_set_destructor();
}
