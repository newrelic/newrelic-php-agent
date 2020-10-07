/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_slab.h"
#include "util_slab_private.h"

#include "tlib_main.h"

#include <sys/mman.h>

static void test_create_destroy(void) {
  size_t size;
  nr_slab_t* slab = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("0 is not a valid object size", nr_slab_create(0, 0));
  tlib_pass_if_null("objects cannot be larger than the initial page size",
                    nr_slab_create(1024 * 1024, 4096));

  nr_slab_destroy(NULL);
  nr_slab_destroy(&slab);

  /*
   * Test : Explicit page sizes.
   */
  for (size = 1; size < 1048576; size *= 2) {
    slab = nr_slab_create(16, size);

    tlib_pass_if_not_null("a valid size must create a slab", slab);
    tlib_pass_if_true(
        "the aligned object size must be equal to or greater than the "
        "requested size",
        slab->object_size >= 16, "slab->object_size=%zu size=%zu",
        slab->object_size, (size_t)16);
    tlib_pass_if_true(
        "a slab must have a page size greater than or equal to the requested "
        "size",
        slab->page_size >= size, "slab->page_size=%zu size=%zu",
        slab->page_size, size);

    tlib_pass_if_not_null("a valid slab must have a page", slab->head);
    tlib_pass_if_size_t_equal("a slab page must have a valid capacity",
                              slab->page_size - sizeof(nr_slab_page_t),
                              slab->head->capacity);

    nr_slab_destroy(&slab);
    tlib_pass_if_null("the slab pointer must be NULLed when destroyed", slab);
  }

  /*
   * Test : Default page sizes.
   */
  for (size = 1; size < 1048576; size *= 2) {
    slab = nr_slab_create(size, 0);

    tlib_pass_if_not_null("a valid size must create a slab", slab);
    tlib_pass_if_true(
        "the aligned object size must be equal to or greater than the "
        "requested size",
        slab->object_size >= size, "slab->object_size=%zu size=%zu",
        slab->object_size, size);
    tlib_pass_if_true(
        "a slab must have a page size at least four times the object size",
        slab->page_size >= (4 * slab->object_size),
        "slab->page_size=%zu slab->object_size=%zu", slab->page_size,
        slab->object_size);

    tlib_pass_if_not_null("a valid slab must have a page", slab->head);
    tlib_pass_if_size_t_equal("a slab page must have a valid capacity",
                              slab->page_size - sizeof(nr_slab_page_t),
                              slab->head->capacity);

    nr_slab_destroy(&slab);
    tlib_pass_if_null("the slab pointer must be NULLed when destroyed", slab);
  }

  nr_slab_destroy(&slab);
}

static void test_next(void) {
  size_t i;
  nr_slab_t* slab;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("a NULL slab must not provide an object",
                    nr_slab_next(NULL));

  slab = nr_slab_create(1024, 0);
  nr_free(slab->head);
  slab->head = NULL;
  tlib_pass_if_null("a slab with a NULL head must not provide an object",
                    nr_slab_next(slab));
  nr_slab_destroy(&slab);

  /*
   * Test : Normal operation.
   */
  slab = nr_slab_create(1024, 0);

  for (i = 0; i < 3 * (slab->page_size / slab->object_size); i++) {
    char* chunk = (char*)nr_slab_next(slab);
    size_t j;

    tlib_pass_if_not_null("a chunk must not be NULL", chunk);

    // Try writing to and reading from every byte to test for invalid writes
    // under valgrind.
    nr_memset(chunk, 42, slab->object_size);
    for (j = 0; j < slab->object_size; j++) {
      // For performance reasons, we'll just do the test and only invoke tlib if
      // there's actually a failure.
      if (42 != chunk[j]) {
        tlib_pass_if_char_equal(
            "each character in the chunk must be the right value", 42,
            chunk[j]);
      }
    }
  }

  tlib_pass_if_not_null(
      "this test must result in at least one more page being allocated",
      slab->head->prev);

  nr_slab_destroy(&slab);
}

static void test_release(void) {
  int* ptr = NULL;
  nr_slab_t* slab = NULL;
  int val = 0;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("a NULL slab must not allow an object to be released",
                          false, nr_slab_release(slab, &val));

  slab = nr_slab_create(sizeof(int), 0);

  tlib_pass_if_bool_equal("a NULL object must not be released", false,
                          nr_slab_release(slab, NULL));

  /*
   * Test : Releasing an item should put it into the free list.
   */
  ptr = (int*)nr_slab_next(slab);
  tlib_pass_if_bool_equal("releasing a valid object must succeed", true,
                          nr_slab_release(slab, ptr));
  tlib_pass_if_size_t_equal(
          "releasing a valid object must put it into the free list", 1,
          nr_vector_size(&slab->free_list));
  tlib_pass_if_ptr_equal(
          "requesting an object with a non-empty free list should result in the "
          "last freed object being returned",
          ptr, nr_slab_next(slab));
  tlib_pass_if_size_t_equal(
          "taking an object out of the free list must empty it", 0,
          nr_vector_size(&slab->free_list));
  tlib_fail_if_ptr_equal(
          "requesting an object with an empty free list should result in a new "
          "object being returned",
          ptr, nr_slab_next(slab));

  nr_slab_destroy(&slab);

}

static void test_count(void) {
  int* ptr_1 = NULL;
  int* ptr_2 = NULL;
  nr_slab_t* slab = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_size_t_equal("0 slab count on NULL slab", 0,
                            nr_slab_count(NULL));

  slab = nr_slab_create(sizeof(int), sizeof(int) * 32);

  tlib_pass_if_size_t_equal("initial slab count", 0, nr_slab_count(slab));

  /*
   * Test : Request objects.
   */
  nr_slab_next(slab);
  tlib_pass_if_size_t_equal("slab count increases by 1", 1,
                            nr_slab_count(slab));

  ptr_1 = nr_slab_next(slab);
  tlib_pass_if_size_t_equal("slab count increases by 1", 2,
                            nr_slab_count(slab));

  ptr_2 = nr_slab_next(slab);
  tlib_pass_if_size_t_equal("slab count increases by 1", 3,
                            nr_slab_count(slab));

  /*
   * Test : Release objects.
   */
  nr_slab_release(slab, ptr_2);
  tlib_pass_if_size_t_equal("slab count unaltered by release", 3,
                            nr_slab_count(slab));

  nr_slab_release(slab, ptr_1);
  tlib_pass_if_size_t_equal("slab count unaltered by release", 3,
                            nr_slab_count(slab));

  /*
   * Test : Request objects again from the free list.
   */
  nr_slab_next(slab);
  nr_slab_next(slab);

  /*
   * Test : Request objects again from the free list.
   */
  tlib_pass_if_size_t_equal("slab count is 5", 5, nr_slab_count(slab));
  tlib_pass_if_size_t_equal("3 used slab objects", 3 * slab->object_size,
                            slab->head->used);

  nr_slab_destroy(&slab);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create_destroy();
  test_next();
  test_release();
  test_count();
}
