/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_sort.h"
#include "util_threads.h"

#include "tlib_main.h"

static nrt_thread_local intptr_t expected_userdata;

static int compare_int(const void* a, const void* b, void* userdata NRUNUSED) {
  tlib_pass_if_int_equal("expected userdata", expected_userdata,
                         (intptr_t)userdata);

  return *((const int*)a) - *((const int*)b);
}

static void test_bad_parameters(void) {
  int a[4] = {0, 1, 2, 3};

  // As nr_sort() doesn't return a value to match qsort_r(), we're just testing
  // that the process doesn't segfault.
  nr_sort(NULL, 4, sizeof(int), compare_int, NULL);
  nr_sort(a, 4, sizeof(int), NULL, NULL);
}

static void test_empty(void) {
  int a[4] = {3, 2, 1, 0};

  nr_sort(a, 0, sizeof(int), compare_int, NULL);
  tlib_pass_if_int_equal("element 0 must be untouched", 3, a[0]);
  tlib_pass_if_int_equal("element 1 must be untouched", 2, a[1]);
  tlib_pass_if_int_equal("element 2 must be untouched", 1, a[2]);
  tlib_pass_if_int_equal("element 3 must be untouched", 0, a[3]);
}

static void test_already_sorted(void) {
  int a[4] = {0, 1, 2, 3};

  nr_sort(a, 4, sizeof(int), compare_int, NULL);
  tlib_pass_if_int_equal("element 0 must be untouched", 0, a[0]);
  tlib_pass_if_int_equal("element 1 must be untouched", 1, a[1]);
  tlib_pass_if_int_equal("element 2 must be untouched", 2, a[2]);
  tlib_pass_if_int_equal("element 3 must be untouched", 3, a[3]);
}

static void test_sort(void) {
  int a[4] = {3, 2, 1, 0};

  expected_userdata = 42;
  nr_sort(a, 4, sizeof(int), compare_int, (void*)42);
  tlib_pass_if_int_equal("element 0 must be sorted", 0, a[0]);
  tlib_pass_if_int_equal("element 1 must be sorted", 1, a[1]);
  tlib_pass_if_int_equal("element 2 must be sorted", 2, a[2]);
  tlib_pass_if_int_equal("element 3 must be sorted", 3, a[3]);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 8, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_empty();
  test_already_sorted();
  test_sort();
}
