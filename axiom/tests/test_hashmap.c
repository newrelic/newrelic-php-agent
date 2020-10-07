/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_hashmap.h"
#include "util_hashmap_private.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

static void destructor(void* value) {
  nr_free(value);
}

static void test_create_destroy(void) {
  nr_hashmap_t* hashmap;

  /*
   * Test : Basic operation.
   */
  hashmap = nr_hashmap_create(NULL);
  tlib_pass_if_not_null("hashmap", hashmap);
  tlib_pass_if_null("hashmap dtor", hashmap->dtor_func);
  tlib_pass_if_size_t_equal("hashmap buckets", 8, hashmap->log2_num_buckets);
  tlib_pass_if_not_null("hashmap buckets", hashmap->buckets);
  nr_hashmap_destroy(&hashmap);

  /*
   * Test : With bucket sizes.
   */
  hashmap = nr_hashmap_create_buckets(16, NULL);
  tlib_pass_if_not_null("hashmap", hashmap);
  tlib_pass_if_size_t_equal("hashmap buckets", 4, hashmap->log2_num_buckets);
  nr_hashmap_destroy(&hashmap);

  hashmap = nr_hashmap_create_buckets(0, NULL);
  tlib_pass_if_not_null("hashmap", hashmap);
  tlib_pass_if_size_t_equal("hashmap buckets", 8, hashmap->log2_num_buckets);
  nr_hashmap_destroy(&hashmap);

  hashmap = nr_hashmap_create_buckets(511, destructor);
  tlib_pass_if_not_null("hashmap", hashmap);
  tlib_pass_if_ptr_equal("hashmap dtor", destructor, hashmap->dtor_func);
  tlib_pass_if_size_t_equal("hashmap buckets", 9, hashmap->log2_num_buckets);
  nr_hashmap_destroy(&hashmap);

  /*
   * Test : Over the limit.
   */
  hashmap = nr_hashmap_create_buckets(1 << 29, destructor);
  tlib_pass_if_not_null("hashmap", hashmap);
  tlib_pass_if_ptr_equal("hashmap dtor", destructor, hashmap->dtor_func);
  tlib_pass_if_size_t_equal("hashmap buckets", 24, hashmap->log2_num_buckets);
  nr_hashmap_destroy(&hashmap);
}

static void apply_func(uint64_t* value,
                       const char* key,
                       size_t key_len,
                       uint64_t* sum) {
  *sum += *value;

  tlib_pass_if_uint64_t_equal("key", *value, *((const uint64_t*)key));
  tlib_pass_if_size_t_equal("key size", sizeof(uint64_t), key_len);
}

static void test_apply(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(NULL);
  uint64_t i;
  uint64_t expected_sum = 0;
  uint64_t sum = 0;
  uint64_t values[1024];

  for (i = 0; i < 1024; i++) {
    values[i] = i;
    expected_sum += i;

    nr_hashmap_index_update(hashmap, i, &values[i]);
  }

  nr_hashmap_apply(hashmap, (nr_hashmap_apply_func_t)apply_func, &sum);
  tlib_pass_if_uint64_t_equal("sum", expected_sum, sum);

  nr_hashmap_destroy(&hashmap);
}

static void test_delete(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL hashmap",
                              nr_hashmap_delete(NULL, "foo", 3));
  tlib_pass_if_status_failure("NULL key", nr_hashmap_delete(hashmap, NULL, 3));
  tlib_pass_if_status_failure("empty key",
                              nr_hashmap_delete(hashmap, "foo", 0));

  /*
   * Test : Non-existent key.
   */
  tlib_pass_if_status_failure("missing key",
                              nr_hashmap_delete(hashmap, NR_PSTR("foo")));

  /*
   * Test : Extant key.
   */
  nr_hashmap_update(hashmap, NR_PSTR("foo"), NULL);
  tlib_fail_if_int_equal("before delete", 0,
                         nr_hashmap_has(hashmap, NR_PSTR("foo")));
  tlib_pass_if_size_t_equal("hashmap size", 1, nr_hashmap_count(hashmap));
  nr_hashmap_delete(hashmap, NR_PSTR("foo"));
  tlib_pass_if_size_t_equal("hashmap size", 0, nr_hashmap_count(hashmap));
  tlib_pass_if_int_equal("after delete", 0,
                         nr_hashmap_has(hashmap, NR_PSTR("foo")));

  nr_hashmap_destroy(&hashmap);
}

static void test_get_set(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(NULL);
  char* value = nr_strdup("test");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL hashmap", nr_hashmap_get(NULL, NR_PSTR("foo")));
  tlib_pass_if_null("NULL key", nr_hashmap_get(hashmap, NULL, 1));
  tlib_pass_if_null("empty key", nr_hashmap_get(hashmap, NR_PSTR("")));

  tlib_pass_if_status_failure("NULL hashmap",
                              nr_hashmap_set(NULL, NR_PSTR("foo"), NULL));
  tlib_pass_if_status_failure("NULL hashmap",
                              nr_hashmap_set(hashmap, NULL, 1, NULL));
  tlib_pass_if_status_failure("NULL hashmap",
                              nr_hashmap_set(hashmap, NR_PSTR(""), NULL));

  tlib_pass_if_size_t_equal("NULL hashmap", 0, nr_hashmap_count(NULL));

  /*
   * Test : nr_hashmap_get on an empty hashmap.
   */
  tlib_pass_if_null("empty hashmap", nr_hashmap_get(hashmap, NR_PSTR("foo")));

  nr_hashmap_update(NULL, NR_PSTR("foo"), NULL);
  nr_hashmap_update(hashmap, NULL, 1, NULL);
  nr_hashmap_update(hashmap, NR_PSTR(""), NULL);

  /*
   * Test : nr_hashmap_set.
   */
  tlib_pass_if_size_t_equal("count", 0, nr_hashmap_count(hashmap));
  tlib_pass_if_status_success("first set",
                              nr_hashmap_set(hashmap, NR_PSTR("foo"), value));
  tlib_pass_if_status_failure("duplicate set",
                              nr_hashmap_set(hashmap, NR_PSTR("foo"), value));
  tlib_pass_if_status_success("second set",
                              nr_hashmap_set(hashmap, NR_PSTR("bar"), NULL));
  tlib_pass_if_size_t_equal("count", 2, nr_hashmap_count(hashmap));

  /*
   * Test : nr_hashmap_get.
   */
  tlib_pass_if_ptr_equal("foo", value, nr_hashmap_get(hashmap, NR_PSTR("foo")));
  tlib_pass_if_ptr_equal("bar", NULL, nr_hashmap_get(hashmap, NR_PSTR("bar")));

  /*
   * Test : nr_hashmap_update.
   */
  nr_hashmap_update(hashmap, NR_PSTR("foo"), NULL);
  tlib_pass_if_ptr_equal("update", NULL,
                         nr_hashmap_get(hashmap, NR_PSTR("foo")));

  nr_hashmap_update(hashmap, NR_PSTR("quux"), value);
  tlib_pass_if_ptr_equal("update", value,
                         nr_hashmap_get(hashmap, NR_PSTR("quux")));
  tlib_pass_if_size_t_equal("count", 3, nr_hashmap_count(hashmap));

  nr_hashmap_destroy(&hashmap);
  nr_free(value);
}

static void test_get_into(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(NULL);
  void* out = &hashmap;
  char* value = nr_strdup("test");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL hashmap", 0,
                         nr_hashmap_get_into(NULL, NR_PSTR("foo"), &out));
  tlib_pass_if_ptr_equal("out is unchanged", &hashmap, out);
  tlib_pass_if_int_equal("NULL key", 0,
                         nr_hashmap_get_into(hashmap, NULL, 1, &out));
  tlib_pass_if_ptr_equal("out is unchanged", &hashmap, out);
  tlib_pass_if_int_equal("empty key", 0,
                         nr_hashmap_get_into(hashmap, NR_PSTR(""), &out));
  tlib_pass_if_ptr_equal("out is unchanged", &hashmap, out);
  tlib_pass_if_int_equal("NULL out", 0,
                         nr_hashmap_get_into(hashmap, NR_PSTR("foo"), NULL));

  /*
   * Test : nr_hashmap_get_into on an empty hashmap.
   */
  tlib_pass_if_int_equal("empty hashmap", 0,
                         nr_hashmap_get_into(hashmap, NR_PSTR("foo"), &out));
  tlib_pass_if_ptr_equal("out is unchanged", &hashmap, out);

  nr_hashmap_set(hashmap, NR_PSTR("foo"), value);
  nr_hashmap_set(hashmap, NR_PSTR("null"), NULL);

  /*
   * Test : nr_hashmap_get_into.
   */
  tlib_pass_if_int_equal("foo", 1,
                         nr_hashmap_get_into(hashmap, NR_PSTR("foo"), &out));
  tlib_pass_if_ptr_equal("foo", value, out);
  tlib_pass_if_int_equal("bar", 0,
                         nr_hashmap_get_into(hashmap, NR_PSTR("bar"), &out));
  tlib_pass_if_ptr_equal("out is unchanged", value, out);
  tlib_pass_if_int_equal("null", 1,
                         nr_hashmap_get_into(hashmap, NR_PSTR("null"), &out));
  tlib_pass_if_ptr_equal("null", NULL, out);

  nr_hashmap_destroy(&hashmap);
  nr_free(value);
}

static void test_has(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL hashmap", 0, nr_hashmap_has(NULL, "foo", 3));
  tlib_pass_if_int_equal("NULL key", 0, nr_hashmap_has(hashmap, NULL, 3));
  tlib_pass_if_int_equal("empty key", 0, nr_hashmap_has(hashmap, "foo", 0));

  /*
   * Test : Non-existent key.
   */
  tlib_pass_if_int_equal("missing key", 0,
                         nr_hashmap_has(hashmap, NR_PSTR("foo")));

  /*
   * Test : Extant key.
   */
  nr_hashmap_update(hashmap, NR_PSTR("foo"), NULL);
  tlib_fail_if_int_equal("after update", 0,
                         nr_hashmap_has(hashmap, NR_PSTR("foo")));

  nr_hashmap_destroy(&hashmap);
}

static void test_stress(void) {
  /*
   * Tests that force bucket overflow. num needs to be divisible by four.
   */
  nr_hashmap_t* hashmap = nr_hashmap_create_buckets(16, destructor);
  uint64_t i;
  const uint64_t num = 4096;

  for (i = 0; i < num; i++) {
    char* value = nr_strdup("foo");

    nr_hashmap_index_set(hashmap, i, value);
  }

  for (i = 0; i < num; i += 2) {
    char* value = (char*)nr_hashmap_index_get(hashmap, i);

    tlib_pass_if_str_equal("value", "foo", value);
  }

  for (i = 0; i < num; i += 4) {
    tlib_pass_if_status_success("delete", nr_hashmap_index_delete(hashmap, i));
  }

  tlib_pass_if_size_t_equal("count", num - (num / 4),
                            nr_hashmap_count(hashmap));

  nr_hashmap_destroy(&hashmap);
}

static void test_update(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(destructor);
  int i;
  int num = 4096;
  int overwrites = 8;

  for (i = 0; i < num; i++) {
    /*
     * Force the update to overwrite values that were heap-allocated, thereby
     * ensuring that we call the destructor on update.
     */
    uint64_t index = num % overwrites;
    char* value = nr_strdup("foo");

    nr_hashmap_index_update(hashmap, index, value);
  }

  nr_hashmap_destroy(&hashmap);
}

static int vector_string_comparator(const void* a,
                                    const void* b,
                                    void* userdata NRUNUSED) {
  return nr_strcmp(a, b);
}

static void test_keys(void) {
  nr_hashmap_t* hashmap = nr_hashmap_create(NULL);
  nr_vector_t* keys;
  char* value = "test";

  tlib_pass_if_null("NULL keys on NULL hashmap", nr_hashmap_keys(NULL));

  /*
   * Add elements.
   */
  tlib_pass_if_status_success("set with key foo",
                              nr_hashmap_set(hashmap, NR_PSTR("foo"), value));
  tlib_pass_if_status_success("set with key bar",
                              nr_hashmap_set(hashmap, NR_PSTR("bar"), value));
  tlib_pass_if_status_success("set with key spam",
                              nr_hashmap_set(hashmap, NR_PSTR("spam"), value));

  /*
   * Check keys.
   */
  keys = nr_hashmap_keys(hashmap);
  tlib_pass_if_not_null("keys are not NULL", keys);
  tlib_pass_if_size_t_equal("3 keys added", 3, nr_vector_size(keys));
  tlib_pass_if_bool_equal(
      "key foo found", true,
      nr_vector_find_first(keys, "foo", vector_string_comparator, NULL, NULL));
  tlib_pass_if_bool_equal(
      "key barfound", true,
      nr_vector_find_first(keys, "bar", vector_string_comparator, NULL, NULL));
  tlib_pass_if_bool_equal(
      "key spam found", true,
      nr_vector_find_first(keys, "spam", vector_string_comparator, NULL, NULL));

  nr_vector_destroy(&keys);
  nr_hashmap_destroy(&hashmap);
}

void test_main(void* p NRUNUSED) {
  test_create_destroy();
  test_apply();
  test_delete();
  test_get_set();
  test_get_into();
  test_has();
  test_keys();
  test_stress();
  test_update();
}
