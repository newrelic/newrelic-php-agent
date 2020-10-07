/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "util_memory.h"
#include "util_string_pool.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_create_destroy(void) {
  nrpool_t* empty = nr_string_pool_create();

  nr_string_pool_destroy(&empty);
  tlib_pass_if_true("pool destroy zeros pointer", 0 == empty, "empty=%p",
                    empty);
}

static void test_str_misc(void) {
  char* lc;

  lc = nr_string_to_lowercase(0);
  tlib_pass_if_true("null pointer", 0 == lc, "lc=%p", lc);
  nr_free(lc);

  lc = nr_string_to_lowercase("");
  tlib_pass_if_true("empty string", 0 != lc, "lc=%p", lc);
  tlib_pass_if_true("empty string", 0 == nr_strcmp("", lc), "lc=%s", lc);
  nr_free(lc);

  lc = nr_string_to_lowercase("ABC");
  tlib_pass_if_true("simple string", 0 != lc, "lc=%p", lc);
  tlib_pass_if_true("simple string", 0 == nr_strcmp("abc", lc), "lc=%s", lc);
  nr_free(lc);

  lc = nr_string_to_lowercase("abc");
  tlib_pass_if_true("simple string", 0 != lc, "lc=%p", lc);
  tlib_pass_if_true("simple string", 0 == nr_strcmp("abc", lc), "lc=%s", lc);
  nr_free(lc);
}

static void test_nr_string_len(void) {
  int rv;
  nrpool_t* empty = nr_string_pool_create();
  nrpool_t* pool = nr_string_pool_create();

  rv = nr_string_add(pool, "alpha");
  tlib_pass_if_true("initial add", 1 == rv, "rv=%d", rv);

  rv = nr_string_len(0, 1);
  tlib_pass_if_true("no pool", -1 == rv, "rv=%d", rv);
  rv = nr_string_len(pool, -1);
  tlib_pass_if_true("negative idx", -1 == rv, "rv=%d", rv);
  rv = nr_string_len(empty, 1);
  tlib_pass_if_true("empty pool", -1 == rv, "rv=%d", rv);
  rv = nr_string_len(pool, 2);
  tlib_pass_if_true("high idx", -1 == rv, "rv=%d", rv);
  rv = nr_string_len(pool, 1);
  tlib_pass_if_true("success", 5 == rv, "rv=%d", rv);

  nr_string_pool_destroy(&empty);
  nr_string_pool_destroy(&pool);
}

static void test_nr_string_hash(void) {
  int rv;
  uint32_t hash;
  nrpool_t* empty = nr_string_pool_create();
  nrpool_t* pool = nr_string_pool_create();

  rv = nr_string_add_with_hash(pool, "alpha", 123);
  tlib_pass_if_true("initial add", 1 == rv, "rv=%d", rv);

  hash = nr_string_hash(0, 1);
  tlib_pass_if_true("no pool", 0 == hash, "hash=%d", hash);
  hash = nr_string_hash(pool, -1);
  tlib_pass_if_true("negative idx", 0 == hash, "hash=%d", hash);
  hash = nr_string_hash(empty, 1);
  tlib_pass_if_true("empty pool", 0 == hash, "hash=%d", hash);
  hash = nr_string_hash(pool, 2);
  tlib_pass_if_true("high idx", 0 == hash, "hash=%d", hash);
  hash = nr_string_hash(pool, 1);
  tlib_pass_if_true("success", 123 == hash, "hash=%d", hash);

  nr_string_pool_destroy(&empty);
  nr_string_pool_destroy(&pool);
}

static void test_nr_string_get(void) {
  int rv;
  const char* string;
  nrpool_t* empty = nr_string_pool_create();
  nrpool_t* pool = nr_string_pool_create();

  rv = nr_string_add(pool, "alpha");
  tlib_pass_if_true("initial add", 1 == rv, "rv=%d", rv);

  string = nr_string_get(0, 1);
  tlib_pass_if_true("no pool", 0 == string, "string=%p", string);
  string = nr_string_get(pool, -1);
  tlib_pass_if_true("negative idx", 0 == string, "string=%p", string);
  string = nr_string_get(empty, 1);
  tlib_pass_if_true("empty pool", 0 == string, "string=%p", string);
  string = nr_string_get(pool, 2);
  tlib_pass_if_true("high idx", 0 == string, "string=%p", string);
  string = nr_string_get(pool, 1);
  tlib_pass_if_true("success", 0 == nr_strcmp("alpha", string), "string=%s",
                    NRSAFESTR(string));

  nr_string_pool_destroy(&empty);
  nr_string_pool_destroy(&pool);
}

static void test_find_add_bad_params(void) {
  int idx;
  nrpool_t* empty = nr_string_pool_create();
  nrpool_t* pool = nr_string_pool_create();

  nr_string_add(pool, "alpha");

  idx = nr_string_add(0, 0);
  tlib_pass_if_true("add null params", 0 == idx, "idx=%d", idx);
  idx = nr_string_add(pool, 0);
  tlib_pass_if_true("add null string", 0 == idx, "idx=%d", idx);
  idx = nr_string_add(0, "alpha");
  tlib_pass_if_true("add null pool", 0 == idx, "idx=%d", idx);

  idx = nr_string_add_with_hash(0, 0, 123);
  tlib_pass_if_true("add with hash null params", 0 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash(pool, 0, 123);
  tlib_pass_if_true("add with hash null string", 0 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash(0, "alpha", 123);
  tlib_pass_if_true("add with hash null pool", 0 == idx, "idx=%d", idx);

  idx = nr_string_add_with_hash_length(0, 0, 123, 5);
  tlib_pass_if_true("add with hash length null params", 0 == idx, "idx=%d",
                    idx);
  idx = nr_string_add_with_hash_length(pool, 0, 123, 5);
  tlib_pass_if_true("add with hash length null string", 0 == idx, "idx=%d",
                    idx);
  idx = nr_string_add_with_hash_length(0, "alpha", 123, 5);
  tlib_pass_if_true("add with hash length null pool", 0 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "alpha", 123, -1);
  tlib_pass_if_true("add with hash length negative length", 0 == idx, "idx=%d",
                    idx);

  idx = nr_string_find(0, 0);
  tlib_pass_if_true("find null params", 0 == idx, "idx=%d", idx);
  idx = nr_string_find(pool, 0);
  tlib_pass_if_true("add null string", 0 == idx, "idx=%d", idx);
  idx = nr_string_find(0, "alpha");
  tlib_pass_if_true("find null pool", 0 == idx, "idx=%d", idx);
  idx = nr_string_find(empty, "alpha");
  tlib_pass_if_true("find empty pool", 0 == idx, "idx=%d", idx);

  idx = nr_string_find_with_hash(0, 0, 123);
  tlib_pass_if_true("find with hash null params", 0 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash(pool, 0, 123);
  tlib_pass_if_true("add null string", 0 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash(0, "alpha", 123);
  tlib_pass_if_true("find with hash null pool", 0 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash(empty, "alpha", 123);
  tlib_pass_if_true("find with hash empty pool", 0 == idx, "idx=%d", idx);

  idx = nr_string_find_with_hash_length(0, 0, 123, 5);
  tlib_pass_if_true("find with hash length null params", 0 == idx, "idx=%d",
                    idx);
  idx = nr_string_find_with_hash_length(pool, 0, 123, 5);
  tlib_pass_if_true("add null string", 0 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(0, "alpha", 123, 5);
  tlib_pass_if_true("find with hash length null pool", 0 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(empty, "alpha", 123, 5);
  tlib_pass_if_true("find with hash length empty pool", 0 == idx, "idx=%d",
                    idx);
  idx = nr_string_find_with_hash_length(pool, "alpha", 123, -1);
  tlib_pass_if_true("find with hash length negative length", 0 == idx, "idx=%d",
                    idx);

  nr_string_pool_destroy(&empty);
  nr_string_pool_destroy(&pool);
}

static void test_find_add(void) {
  int idx;
  nrpool_t* pool;

  /*
   * Test : Add and find strings with different hashes, lengths, and bytes.
   */
  pool = nr_string_pool_create();
  idx = nr_string_add_with_hash_length(pool, "alpha", 123, 5);
  tlib_pass_if_true("add", 1 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "Alpha", 123,
                                       5); /* Different bytes */
  tlib_pass_if_true("add", 2 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "alpha", 234,
                                       5); /* Different hash */
  tlib_pass_if_true("add", 3 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "alpha", 123,
                                       4); /* Different length */
  tlib_pass_if_true("add", 4 == idx, "idx=%d", idx);

  idx = nr_string_find_with_hash_length(pool, "alpha", 123, 5);
  tlib_pass_if_true("find", 1 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "Alpha", 123, 5);
  tlib_pass_if_true("find", 2 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "alpha", 234, 5);
  tlib_pass_if_true("find", 3 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "alpha", 123, 4);
  tlib_pass_if_true("find", 4 == idx, "idx=%d", idx);
  nr_string_pool_destroy(&pool);

  /*
   * Test : Adding same string returns same value.
   */
  pool = nr_string_pool_create();
  idx = nr_string_add(pool, "alpha");
  tlib_pass_if_true("add", 1 == idx, "idx=%d", idx);
  idx = nr_string_add(pool, "alpha\0\0\0");
  tlib_pass_if_true("add again", 1 == idx, "idx=%d", idx);
  idx = nr_string_find(pool, "alpha");
  tlib_pass_if_true("find", 1 == idx, "idx=%d", idx);
  nr_string_pool_destroy(&pool);

  pool = nr_string_pool_create();
  idx = nr_string_add(pool, "");
  tlib_pass_if_true("add", 1 == idx, "idx=%d", idx);
  idx = nr_string_add(pool, "");
  tlib_pass_if_true("add again", 1 == idx, "idx=%d", idx);
  idx = nr_string_find(pool, "");
  tlib_pass_if_true("find", 1 == idx, "idx=%d", idx);
  nr_string_pool_destroy(&pool);

  /*
   * Test : Table handles hash collisions.
   */
  pool = nr_string_pool_create();
  idx = nr_string_add_with_hash_length(pool, "a", 123, 1);
  tlib_pass_if_true("add", 1 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "b", 123, 1);
  tlib_pass_if_true("add", 2 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "c", 123, 1);
  tlib_pass_if_true("add", 3 == idx, "idx=%d", idx);
  idx = nr_string_add_with_hash_length(pool, "d", 123, 1);
  tlib_pass_if_true("add", 4 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "a", 123, 1);
  tlib_pass_if_true("find", 1 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "b", 123, 1);
  tlib_pass_if_true("find", 2 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "c", 123, 1);
  tlib_pass_if_true("find", 3 == idx, "idx=%d", idx);
  idx = nr_string_find_with_hash_length(pool, "d", 123, 1);
  tlib_pass_if_true("find", 4 == idx, "idx=%d", idx);
  nr_string_pool_destroy(&pool);
}

const char* example_strings[] = {"UDS",
                                 "only",
                                 "1",
                                 "1-65534,",
                                 "1023",
                                 "<code>newreliccfg</code>",
                                 "If",
                                 "In",
                                 "Please",
                                 "Sets",
                                 "TCP",
                                 "This",
                                 "UNIX",
                                 "a",
                                 "absolute",
                                 "agent",
                                 "also",
                                 "an",
                                 "and",
                                 "are",
                                 "as",
                                 "be",
                                 "by",
                                 "can",
                                 "case",
                                 "communicating",
                                 "communications",
                                 "configured",
                                 "daemon",
                                 "domain",
                                 "endpoint",
                                 "fact",
                                 "file",
                                 "first",
                                 "for",
                                 "form,",
                                 "forms",
                                 "if",
                                 "in",
                                 "is",
                                 "mechanism",
                                 "name",
                                 "no",
                                 "non-standard",
                                 "not",
                                 "note",
                                 "number",
                                 "of",
                                 "operating",
                                 "path",
                                 "paths",
                                 "please",
                                 "port",
                                 "ports",
                                 "preferred",
                                 "provide",
                                 "range",
                                 "relative",
                                 "remember",
                                 "require",
                                 "restriction",
                                 "run",
                                 "second",
                                 "sets",
                                 "setting",
                                 "socket",
                                 "specified",
                                 "specify",
                                 "standard",
                                 "startup",
                                 "string",
                                 "super-user",
                                 "system",
                                 "that",
                                 "the",
                                 "then",
                                 "this",
                                 "through",
                                 "to",
                                 "two",
                                 "use",
                                 "used",
                                 "uses",
                                 "using",
                                 "valid,",
                                 "variable",
                                 "where",
                                 "will",
                                 "with",
                                 "you",
                                 0};

const char* underscore_example_strings[] = {"_UDS",
                                            "_only",
                                            "_1",
                                            "_1-65534,",
                                            "_1023",
                                            "_<code>newreliccfg</code>",
                                            "_If",
                                            "_In",
                                            "_Please",
                                            "_Sets",
                                            "_TCP",
                                            "_This",
                                            "_UNIX",
                                            "_a",
                                            "_absolute",
                                            "_agent",
                                            "_also",
                                            "_an",
                                            "_and",
                                            "_are",
                                            "_as",
                                            "_be",
                                            "_by",
                                            "_can",
                                            "_case",
                                            "_communicating",
                                            "_communications",
                                            "_configured",
                                            "_daemon",
                                            "_domain",
                                            "_endpoint",
                                            "_fact",
                                            "_file",
                                            "_first",
                                            "_for",
                                            "_form,",
                                            "_forms",
                                            "_if",
                                            "_in",
                                            "_is",
                                            "_mechanism",
                                            "_name",
                                            "_no",
                                            "_non-standard",
                                            "_not",
                                            "_note",
                                            "_number",
                                            "_of",
                                            "_operating",
                                            "_path",
                                            "_paths",
                                            "_please",
                                            "_port",
                                            "_ports",
                                            "_preferred",
                                            "_provide",
                                            "_range",
                                            "_relative",
                                            "_remember",
                                            "_require",
                                            "_restriction",
                                            "_run",
                                            "_second",
                                            "_sets",
                                            "_setting",
                                            "_socket",
                                            "_specified",
                                            "_specify",
                                            "_standard",
                                            "_startup",
                                            "_string",
                                            "_super-user",
                                            "_system",
                                            "_that",
                                            "_the",
                                            "_then",
                                            "_this",
                                            "_through",
                                            "_to",
                                            "_two",
                                            "_use",
                                            "_used",
                                            "_uses",
                                            "_using",
                                            "_valid,",
                                            "_variable",
                                            "_where",
                                            "_will",
                                            "_with",
                                            "_you",
                                            0};

static void test_add_find(void) {
  int rv_length;
  nrpool_t* in = nr_string_pool_create();
  int i;

  for (i = 0; example_strings[i]; i++) {
    rv_length = nr_string_add(in, example_strings[i]);
    tlib_pass_if_true("add string", (1 + i) == rv_length, "i=%d rv_length=%d",
                      i, rv_length);
  }

  for (i = 0; example_strings[i]; i++) {
    rv_length = nr_string_find(in, example_strings[i]);
    tlib_pass_if_true("find string", (1 + i) == rv_length, "i=%d rv_length=%d",
                      i, rv_length);
  }

  for (i = 0; underscore_example_strings[i]; i++) {
    rv_length = nr_string_find(in, underscore_example_strings[i]);
    tlib_pass_if_true("find absent string", 0 == rv_length, "i=%d rv_length=%d",
                      i, rv_length);
  }

  nr_string_pool_destroy(&in);
}

static void test_trigger_realloc(void) {
  int rv_length;
  nrpool_t* in = nr_string_pool_create();
  int i;
  char string[128];
  int limit = NR_STRPOOL_STARTING_SIZE + NR_STRPOOL_INCREASE_SIZE + 5;

  for (i = 0; i < limit; i++) {
    snprintf(string, sizeof(string), "example%dstring%d", i, i);
    rv_length = nr_string_add(in, string);
    tlib_pass_if_true("add string", (1 + i) == rv_length, "i=%d rv_length=%d",
                      i, rv_length);
  }

  for (i = 0; i < limit; i++) {
    snprintf(string, sizeof(string), "example%dstring%d", i, i);
    rv_length = nr_string_find(in, string);
    tlib_pass_if_true("find string", (1 + i) == rv_length, "i=%d rv_length=%d",
                      i, rv_length);
  }

  nr_string_pool_destroy(&in);
}

static void test_large_string(void) {
  int i;
  int idx;
  nrpool_t* in = nr_string_pool_create();
  int size = 2 * NR_STRPOOL_TABLE_SIZE;
  char* string = (char*)nr_malloc(size + 1);

  string[size] = '\0';
  for (i = 0; i < size; i++) {
    string[i] = 'a';
  }

  idx = nr_string_add(in, string);
  tlib_pass_if_true("add large string", 1 == idx, "idx=%d", idx);

  idx = nr_string_find(in, string);
  tlib_pass_if_true("find large string", 1 == idx, "idx=%d", idx);

  nr_free(string);
  nr_string_pool_destroy(&in);
}

static void test_pool_to_json(void) {
  char* json;
  nrpool_t* empty = nr_string_pool_create();
  nrpool_t* pool = nr_string_pool_create();

  json = nr_string_pool_to_json(0);
  tlib_pass_if_true("null pool", 0 == json, "json=%p", json);

  json = nr_string_pool_to_json(empty);
  tlib_pass_if_true("empty pool", 0 == nr_strcmp("[]", json), "json=%s",
                    NRSAFESTR(json));
  nr_free(json);

  nr_string_add(pool, "alpha");
  nr_string_add(pool, "beta");
  nr_string_add(pool, "alpha");
  nr_string_add(pool, "gamma");
  nr_string_add(pool, "beta");
  json = nr_string_pool_to_json(pool);
  tlib_pass_if_true("normal pool",
                    0 == nr_strcmp("[\"alpha\",\"beta\",\"gamma\"]", json),
                    "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_string_pool_destroy(&empty);
  nr_string_pool_destroy(&pool);
}

typedef struct {
  size_t current;
  const char** strings;
} apply_state_t;

static void test_apply_callback(const char* str,
                                int len,
                                apply_state_t* state) {
  const char* expected = state->strings[state->current];

  tlib_pass_if_str_equal("apply callback string", expected, str);
  tlib_pass_if_int_equal("apply callback length", nr_strlen(expected), len);

  state->current++;
}

static void test_apply_callback_never(const char* str,
                                      int len,
                                      void* data NRUNUSED) {
  tlib_pass_if_true("unexpected callback", 0, "str: %.*s", len, str);
}

static void test_apply(void) {
  nrpool_t* pool;
  apply_state_t state = {
    .current = 0,
    .strings = (const char *[]) {
      "alpha",
      "beta",
      "gamma",
    },
  };

  /*
   * Test : Invalid arguments. Do we crash?
   */
  nr_string_pool_apply(NULL, test_apply_callback_never, NULL);

  /*
   * Test : Empty pool.
   */
  pool = nr_string_pool_create();
  nr_string_pool_apply(pool, test_apply_callback_never, NULL);

  /*
   * Test : Normal operation.
   */
  nr_string_add(pool, "alpha");
  nr_string_add(pool, "beta");
  nr_string_add(pool, "alpha");
  nr_string_add(pool, "gamma");
  nr_string_add(pool, "beta");

  nr_string_pool_apply(pool, (nr_string_pool_apply_func_t)test_apply_callback,
                       &state);

  nr_string_pool_destroy(&pool);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_str_misc();

  test_create_destroy();
  test_nr_string_len();
  test_nr_string_hash();
  test_nr_string_get();
  test_find_add_bad_params();
  test_find_add();

  test_add_find();
  test_trigger_realloc();
  test_large_string();

  test_pool_to_json();
  test_apply();
}
