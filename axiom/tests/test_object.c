/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/types.h>

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "util_memory.h"
#include "util_number_converter.h"
#include "util_object.h"
#include "util_object_private.h"
#include "util_strings.h"

#include "tlib_main.h"

/*
 * Some O/S (centos 5.5) don't define these symbols in the normal way.
 * rather than monkey around with (re)defining STDC_VERSION, just define
 * plausible replacements.
 */
#if !defined(ULLONG_MAX)
#define ULLONG_MAX 0xffffffffffffffffULL
#endif
#if !defined(LLONG_MAX)
#define LLONG_MAX 0x7fffffffffffffffLL
#endif
#if !defined(LLONG_MIN)
#define LLONG_MIN (-0x7fffffffffffffffLL - 1)
#endif

nrotype_t otypes[] = {
    NR_OBJECT_INVALID, NR_OBJECT_NONE,  NR_OBJECT_BOOLEAN, NR_OBJECT_INT,
    NR_OBJECT_LONG,    NR_OBJECT_ULONG, NR_OBJECT_DOUBLE,  NR_OBJECT_JSTRING,
    NR_OBJECT_STRING,  NR_OBJECT_HASH,  NR_OBJECT_ARRAY,
};

int num_otypes = sizeof(otypes) / sizeof(nrotype_t);

#define nro_test(T, O, C)                                                   \
  {                                                                         \
    int strcmp_result;                                                      \
                                                                            \
    dump = nro_dump((O));                                                   \
    strcmp_result = nr_strcmp(dump, C);                                     \
    tlib_pass_if_true(T, 0 == strcmp_result,                                \
                      "strcmp_result=%d dump=%s correct=%s", strcmp_result, \
                      dump, C);                                             \
    nr_free(dump);                                                          \
  }

#define nro_test_new(O, C) \
  {                        \
    ob = O;                \
    nro_test(#O, ob, C);   \
    nro_delete(ob);        \
  }

#define INT_TEST(S, G, C)                                            \
  {                                                                  \
    nrobj_t* intob = nro_new_int((S));                               \
    int gotten_int;                                                  \
    gotten_int = nro_get_int(NULL, &err);                            \
    tlib_pass_if_true("int get fails OK",                            \
                      (-1 == gotten_int) && (NR_FAILURE == err),     \
                      "gotten_int=%d err=%d", gotten_int, (int)err); \
    gotten_int = nro_get_int(intob, &err);                           \
    tlib_pass_if_true("int get success",                             \
                      ((G) == C gotten_int) && (NR_SUCCESS == err),  \
                      "gotten_int=%d err=%d", gotten_int, (int)err); \
    nro_delete(intob);                                               \
  }

#define LONG_TEST(S, G, C)                                                   \
  {                                                                          \
    nrobj_t* longob = nro_new_long((S));                                     \
    int64_t gotten_long;                                                     \
    gotten_long = nro_get_long(NULL, &err);                                  \
    tlib_pass_if_true(                                                       \
        "long get fails OK", (-1L == gotten_long) && (NR_FAILURE == err),    \
        "gotten_long=" NR_INT64_FMT " err=%d", gotten_long, (int)err);       \
    gotten_long = nro_get_long(longob, &err);                                \
    tlib_pass_if_true(                                                       \
        "long set correctly", ((G) == C gotten_long) && (NR_SUCCESS == err), \
        "gotten_long=" NR_INT64_FMT " err=%d", gotten_long, (int)err);       \
    nro_delete(longob);                                                      \
  }

#define ULONG_TEST(S, G, C)                                                    \
  {                                                                            \
    nrobj_t* ulongob = nro_new_ulong((S));                                     \
    uint64_t gotten_ulong;                                                     \
    gotten_ulong = nro_get_ulong(NULL, &err);                                  \
    tlib_pass_if_true(                                                         \
        "ulong get fails OK", (0 == gotten_ulong) && (NR_FAILURE == err),      \
        "gotten_ulong=" NR_UINT64_FMT " err=%d", gotten_ulong, (int)err);      \
    gotten_ulong = nro_get_ulong(ulongob, &err);                               \
    tlib_pass_if_true(                                                         \
        "ulong set correctly", ((G) == C gotten_ulong) && (NR_SUCCESS == err), \
        "gotten_ulong=" NR_UINT64_FMT " err=%d", gotten_ulong, (int)err);      \
    nro_delete(ulongob);                                                       \
  }

#define DOUBLE_TEST(S, G, C)                                                \
  {                                                                         \
    double gotten_double;                                                   \
    nrobj_t* doubleob = nro_new_double((S));                                \
    gotten_double = nro_get_double(NULL, &err);                             \
    tlib_pass_if_true("double get fails OK",                                \
                      (gotten_double == -1.0) && (NR_FAILURE == err),       \
                      "gotten_double=%lf err=%d", gotten_double, (int)err); \
    gotten_double = nro_get_double(doubleob, &err);                         \
    tlib_pass_if_true("double set correctly",                               \
                      ((G) == C gotten_double) && (NR_SUCCESS == err),      \
                      "gotten_double=%lf err=%d", gotten_double, (int)err); \
    nro_delete(doubleob);                                                   \
  }

static void test_find_array_int(void) {
  nrobj_t* ob;
  int array_position;

  array_position = nro_find_array_int(0, 123);
  tlib_pass_if_true("zero input", -1 == array_position, "array_position=%d",
                    array_position);

  ob = nro_new_hash();
  array_position = nro_find_array_int(ob, 123);
  tlib_pass_if_true("wrong type", -1 == array_position, "array_position=%d",
                    array_position);
  nro_delete(ob);

  ob = nro_new_array();

  array_position = nro_find_array_int(ob, 123);
  tlib_pass_if_true("empty array", -1 == array_position, "array_position=%d",
                    array_position);

  nro_set_array_long(ob, 0, 123);
  array_position = nro_find_array_int(ob, 123);
  tlib_pass_if_true("long not int", -1 == array_position, "array_position=%d",
                    array_position);

  nro_set_array_ulong(ob, 0, 123);
  array_position = nro_find_array_int(ob, 123);
  tlib_pass_if_true("ulong not int", -1 == array_position, "array_position=%d",
                    array_position);

  nro_set_array_int(ob, 0, 456);
  array_position = nro_find_array_int(ob, 123);
  tlib_pass_if_true("wrong int", -1 == array_position, "array_position=%d",
                    array_position);

  nro_set_array_int(ob, 0, 123);
  array_position = nro_find_array_int(ob, 123);
  tlib_pass_if_true("success", 4 == array_position, "array_position=%d",
                    array_position);

  nro_delete(ob);
}

static void test_incomensurate_get(void) {
  nrobj_t* ob;
  nrobj_t* oi;
  const nrobj_t* oh;
  int rv;
  int64_t lv;
  uint64_t ulv;
  double dv;
  const char* sv;
  nr_status_t err;

  ob = nro_new(NR_OBJECT_BOOLEAN);
  oi = nro_new(NR_OBJECT_INT);

  rv = nro_get_boolean(oi, &err);
  tlib_pass_if_true("get boolean failure", -1 == rv, "rv=%d", rv);

  rv = nro_get_int(ob, &err);
  tlib_pass_if_true("get int failure", -1 == rv, "rv=%d", rv);

  lv = nro_get_long(ob, &err);
  tlib_pass_if_true("get long failure", -1LL == lv, "lv=" NR_INT64_FMT, lv);

  ulv = nro_get_ulong(ob, &err);
  tlib_pass_if_true("get ulong failure", 0 == ulv, "ulv=" NR_UINT64_FMT, ulv);

  sv = nro_get_string(ob, &err);
  tlib_pass_if_true("get string failure", NULL == sv, "sv=%s",
                    sv ? sv : "<null>");

  dv = nro_get_double(ob, &err);
  tlib_pass_if_true("get double failure", -1.0 == dv, "dv=%f", dv);

  oh = nro_get_hash_value(NULL, "a", &err);
  tlib_pass_if_true("get hash by value failure", NULL == oh, "oh=%p", oh);

  oh = nro_get_hash_value(ob, "a", &err);
  tlib_pass_if_true("get hash by value failure", NULL == oh, "oh=%p", oh);

  oh = nro_get_hash_value_by_index(NULL, 0, &err, (const char**)NULL);
  tlib_pass_if_true("get hash by index failure", NULL == oh, "oh=%p", oh);

  oh = nro_get_hash_value_by_index(ob, 0, &err, (const char**)NULL);
  tlib_pass_if_true("get hash by index failure", NULL == oh, "oh=%p", oh);

  nro_delete(ob);
  nro_delete(oi);
}

static void test_nro_getival(void) {
  nrobj_t* ob;
  int rv;
  nr_status_t err;

  ob = nro_new_int(3);
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival int extraction", NR_SUCCESS == err, "err=%d",
                    (int)err);
  tlib_pass_if_true("nro_get_ival int extraction", 3 == rv, "rv=%d", rv);
  nro_delete(ob);

  ob = nro_new_boolean(1);
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival bool extraction", NR_SUCCESS == err, "err=%d",
                    (int)err);
  tlib_pass_if_true("nro_get_ival bool extraction", 1 == rv, "rv=%d", rv);
  nro_delete(ob);

  ob = nro_new_long(1U << 31);
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival long extraction", NR_SUCCESS == err, "err=%d",
                    (int)err);
  tlib_pass_if_true("nro_get_ival long extraction", (1U << 31) == (unsigned)rv,
                    "rv=%d", rv);
  nro_delete(ob);

  ob = nro_new_ulong(1U << 31);
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival ulong extraction", NR_SUCCESS == err,
                    "err=%d", (int)err);
  tlib_pass_if_true("nro_get_ival ulong extraction", (1U << 31) == (unsigned)rv,
                    "rv=%d", rv);
  nro_delete(ob);

  ob = nro_new_double(2.9);
  tlib_pass_if_true("nro_get_ival double created", 0 != ob, "ob=%p", ob);
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival double extraction", NR_SUCCESS == err,
                    "err=%d", (int)err);
  tlib_pass_if_true("nro_get_ival double extraction", 2 == rv, "rv=%d", rv);
  nro_delete(ob);

  ob = nro_new_double(-2.9);
  tlib_pass_if_true("nro_get_ival double created", 0 != ob, "ob=%p", ob);
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival double extraction", NR_SUCCESS == err,
                    "err=%d", (int)err);
  tlib_pass_if_true("nro_get_ival double extraction", -2 == rv, "rv=%d", rv);
  nro_delete(ob);

  ob = nro_new_array();
  rv = nro_get_ival(ob, &err);
  tlib_pass_if_true("nro_get_ival array extraction", NR_FAILURE == err,
                    "err=%d", (int)err);
  tlib_pass_if_true("nro_get_ival array extraction", -1 == rv, "rv=%d", rv);
  nro_delete(ob); /* should delete array and its contents */
}

static nr_status_t hash_visitor(const char* key, const nrobj_t* val, void* vp) {
  nr_status_t err;
  int intval;
  int* hash_visitor_visits = (int*)vp;

  intval = nro_get_int(val, &err);
  *hash_visitor_visits += 1;
  tlib_pass_if_true("key matches structure", 0 == nr_strncmp("key", key, 3),
                    "key=%s", key);
  tlib_pass_if_true("value in range", (0 <= intval) && (intval < 10),
                    "intval=%d", intval);
  return NR_SUCCESS;
}

static nr_status_t hash_visitor_fails(const char* key,
                                      const nrobj_t* val,
                                      void* vp) {
  int* hash_visitor_visits = (int*)vp;

  (void)key;
  (void)val;
  *hash_visitor_visits += 1;
  if (*hash_visitor_visits == 3) {
    return NR_FAILURE;
  } else {
    return NR_SUCCESS;
  }
}

static void test_nro_iteratehash(void) {
  nrobj_t* ob;
  nrobj_t* hash;
  int i;
  int hash_visitor_visits = 0;

  ob = nro_new(NR_OBJECT_BOOLEAN);
  hash = nro_new_hash();
  for (i = 0; i < 10; i++) {
    char key[10];
    snprintf(key, sizeof(key), "key%d", i);
    nro_set_hash_int(hash, key, i);
  }
  hash_visitor_visits = 0;
  nro_iteratehash(hash, hash_visitor, &hash_visitor_visits);
  tlib_pass_if_int_equal("test_nro_iteratehash visit count", 10,
                         hash_visitor_visits);

  /*
   * Iterator function indicates premature return
   */
  hash_visitor_visits = 0;
  nro_iteratehash(hash, hash_visitor_fails, &hash_visitor_visits);
  tlib_pass_if_int_equal("test_nro_iteratehash visit count", 3,
                         hash_visitor_visits);

  /*
   * error conditions
   */
  hash_visitor_visits = 0;
  nro_iteratehash(NULL, hash_visitor,
                  &hash_visitor_visits); /* null hash table */
  tlib_pass_if_int_equal("test_nro_iteratehash visit count", 0,
                         hash_visitor_visits);

  hash_visitor_visits = 0;
  nro_iteratehash(hash, 0, &hash_visitor_visits); /* null funarg */
  tlib_pass_if_int_equal("test_nro_iteratehash visit count", 0,
                         hash_visitor_visits);

  hash_visitor_visits = 0;
  nro_iteratehash(ob, hash_visitor_fails, &hash_visitor_visits);
  tlib_pass_if_int_equal("test_nro_iteratehash visit count", 0,
                         hash_visitor_visits);

  hash_visitor_visits = 0;
  nro_iteratehash(ob, 0, &hash_visitor_visits);
  tlib_pass_if_int_equal("test_nro_iteratehash visit count", 0,
                         hash_visitor_visits);

  nro_delete(hash);
  nro_delete(ob);
}

static void test_nro_hash_corner_cases(void) {
  nrobj_t* hash;
  nrobj_t* obj;
  const char* key;
  nr_status_t setcode;
  nr_status_t err;
  const nrobj_t* gotten;

  hash = nro_new_hash();
  obj = nro_new_boolean(1); /* will be owned by the hash table */
  nro_set_hash(hash, "qrs", obj);

  gotten
      = nro_get_hash_value(obj, NULL, &err); /* not a hash table, so an error */
  tlib_pass_if_true("test_nro_hash_corner_cases", NULL == gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases", NR_FAILURE == err, "err=%d",
                    (int)err);

  gotten = nro_get_hash_value(hash, "qrs", &err);
  tlib_pass_if_true("test_nro_hash_corner_cases", 0 != gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases", NR_SUCCESS == err, "err=%d",
                    (int)err);

  gotten = nro_get_hash_value(hash, "notfound", &err);
  tlib_pass_if_true("test_nro_hash_corner_cases", NULL == gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases", NR_SUCCESS == err, "err=%d",
                    (int)err);

  gotten = nro_get_hash_value(hash, NULL, &err);
  tlib_pass_if_true("test_nro_hash_corner_cases", NULL == gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases", NR_FAILURE == err, "err=%d",
                    (int)err);

  gotten = nro_get_hash_value_by_index(NULL, 0, &err, &key);
  tlib_pass_if_true("test_nro_hash_corner_cases null hash", NULL == gotten,
                    "gotten=%p", gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases null hash", NR_FAILURE == err,
                    "err=%d", (int)err);

  gotten = nro_get_hash_value_by_index(obj, 0, &err, &key);
  tlib_pass_if_true("test_nro_hash_corner_cases not a hash", NULL == gotten,
                    "gotten=%p", gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases not a hash", NR_FAILURE == err,
                    "err=%d", (int)err);

  gotten = nro_get_hash_value_by_index(hash, 0, &err, &key);
  tlib_pass_if_true("test_nro_hash_corner_cases out of bounds", NULL == gotten,
                    "gotten=%p", gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases out of bounds",
                    NR_FAILURE == err, "err=%d", (int)err);

  gotten = nro_get_hash_value_by_index(hash, 100, &err, &key);
  tlib_pass_if_true("test_nro_hash_corner_cases out of bounds", NULL == gotten,
                    "gotten=%p", gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases out of bounds",
                    NR_FAILURE == err, "err=%d", (int)err);

  gotten = nro_get_hash_value_by_index(hash, 1, &err, &key);
  tlib_pass_if_true("test_nro_hash_corner_cases out of bounds", 0 != gotten,
                    "gotten=%p", gotten);
  tlib_pass_if_true("test_nro_hash_corner_cases out of bounds",
                    NR_SUCCESS == err, "err=%d", (int)err);

  /* can't add a null to an hash */
  setcode = nro_set_hash(hash, "foo", NULL);
  tlib_pass_if_true("test_nro_hash_corner_cases", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);

  nro_delete(obj);
  nro_delete(hash);
}

static void test_nro_array_corner_cases(void) {
  nrobj_t* array;
  const nrobj_t* gotten;
  nr_status_t setcode;
  nr_status_t err;

  array = nro_new_array();
  nro_set_array_boolean(array, 1, 1);

  gotten = nro_get_array_value(NULL, 1, &err);
  tlib_pass_if_true("test_nro_array_corner_cases", NULL == gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_array_corner_cases", NR_FAILURE == err, "err=%d",
                    (int)err);

  gotten = nro_get_array_value(array, 1, &err);
  tlib_pass_if_true("test_nro_array_corner_cases", 0 != gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_array_corner_cases", NR_SUCCESS == err, "err=%d",
                    (int)err);

  gotten = nro_get_array_value(array, 0, &err);
  tlib_pass_if_true("test_nro_array_corner_cases", NULL == gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_array_corner_cases", NR_FAILURE == err, "err=%d",
                    (int)err);

  gotten = nro_get_array_value(array, 2, &err);
  tlib_pass_if_true("test_nro_array_corner_cases", NULL == gotten, "gotten=%p",
                    gotten);
  tlib_pass_if_true("test_nro_array_corner_cases", NR_FAILURE == err, "err=%d",
                    (int)err);

  /* can't add a null to an array */
  setcode = nro_set_array(array, 2, NULL);
  tlib_pass_if_true("test_nro_array_corner_cases", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);

  nro_delete(array);
}

/*
 * Return an empirically well crafted (hash) object that contains one level of
 * hash nesting, large doubles, and other perversities. The caller owns what is
 * returned.
 */
static nrobj_t* construct_hairy_object(void) {
  nrobj_t* hash_src = nro_new_hash();
  nrobj_t* oi;

  nro_set_hash_none(hash_src, "none");
  nro_set_hash_boolean(hash_src, "true", 1);
  nro_set_hash_boolean(hash_src, "false", 0);
  nro_set_hash_int(hash_src, "int", 1 << 30);
  nro_set_hash_long(hash_src, "long", 1LL << 62);
  nro_set_hash_ulong(hash_src, "ulong", UINT64_MAX);
  nro_set_hash_double(hash_src, "double_pi", 3.14159);
  nro_set_hash_double(hash_src, "double_tiny", 3.0e-100);
  nro_set_hash_double(
      hash_src, "double_posinf",
      nr_strtod("Inf", 0)); /* stock json doesn't support ieee abbrevs */
  nro_set_hash_double(
      hash_src, "double_neginf",
      nr_strtod("-Inf", 0)); /* stock json doesn't support ieee abbrevs */
  nro_set_hash_double(
      hash_src, "double_nan",
      nr_strtod("NaN", 0)); /* stock json doesn't support ieee abbrevs */
  nro_set_hash_double(hash_src, "double_neg0", nr_strtod("-0.0", 0));
  nro_set_hash_double(hash_src, "double_pos0", nr_strtod("0.0", 0));

  oi = nro_new_array();
  nro_set_array_int(oi, 1, 0);
  nro_set_array_int(oi, 2, 0);
  nro_set_hash(hash_src, "array", oi);
  nro_delete(oi);

  oi = nro_new_hash();
  nro_set_hash_int(oi, "hash1", 0);
  nro_set_hash_int(oi, "hash2", 0);
  nro_set_hash(hash_src, "hash", oi);
  nro_delete(oi);

  return hash_src;
}

/*
 * Exercise object copy with a nefariously constructed object.
 * This also excercises nro_to_json for the nefarious object.
 */
static void test_nro_hairy_object_json(void) {
  nrobj_t* hash_src;
  nrobj_t* hash_dst;
  char* str_src;
  char* str_dst;
  const char* expect_json;

  hash_src = construct_hairy_object();
  hash_dst = nro_copy(hash_src);
  str_src = nro_to_json(hash_src);
  str_dst = nro_to_json(hash_dst);
  tlib_pass_if_true("test_nro_hairy_object_json",
                    0 == nr_strcmp(str_src, str_dst), "str_src=%s str_dst=%s",
                    str_src, str_dst);

  expect_json
      = "{"
        "\"none\":null,"
        "\"true\":true,"
        "\"false\":false,"
        "\"int\":1073741824,"
        "\"long\":4611686018427387904,"
        "\"ulong\":18446744073709551615,"
        "\"double_pi\":3.14159,"
        "\"double_tiny\":0.00000,"
        "\"double_posinf\":inf,"
        "\"double_neginf\":-inf,"
        "\"double_nan\":nan,"
        "\"double_neg0\":-0.00000,"
        "\"double_pos0\":0.00000,"
        "\"array\":[0,0],"
        "\"hash\":{\"hash1\":0,\"hash2\":0}"
        "}";

  tlib_pass_if_true("test_nro_hairy_object_json copy/json",
                    0 == nr_stricmp(expect_json, str_src),
                    "expect_json=>\n%s\nstr_src=>\n%s", expect_json, str_src);

  nro_delete(hash_dst);
  nro_delete(hash_src);
  nr_free(str_src);
  nr_free(str_dst);
}

/*
 * Return an empirically well crafted (hash) object that contains one
 * level of hash nesting, with utf8 characters that have to go through
 * the json encoder. Make the obj we build have a json length that
 * exceeds 4096, which is the buffer allocation size and extension size.
 *
 * The caller owns what is returned.
 */

static nrobj_t* construct_hairy_utf8_object(int N) {
  int i = 0;
  nrobj_t* hash_src = nro_new_hash();
  nrobj_t* oi = nro_new_array();

  nro_set_array_int(oi, 1, 0);
  nro_set_array_int(oi, 2, 0);
  nro_set_hash(hash_src, "array", oi);
  nro_delete(oi);

  oi = nro_new_hash();
  for (i = 0; i < N; i++) {
    int j;
    int lg;
    char buf_key[1024];
    char buf_val[1024];

    snprintf(buf_key, sizeof(buf_key), "%d", i);
    buf_val[0] = 0;
    for (j = 0; j < i; j++) {
      lg = nr_strlen(buf_val);
      snprintf(buf_val + lg, sizeof(buf_val) - lg, "%s", "😂");
    }
    nro_set_hash_string(oi, buf_key, buf_val);
  }

  nro_set_hash(hash_src, "hash", oi);
  nro_delete(oi);

  return hash_src;
}

static void test_nro_hairy_utf8_object_json(void) {
  nrobj_t* hash_src;
  nrobj_t* hash_dst;
  char* str_src;
  char* str_dst;
  char expect_json[1024 * 64];
  int lg;
  int i;
  const char* sep = "";
  int N = 100;

  hash_src = construct_hairy_utf8_object(N);
  hash_dst = nro_copy(hash_src);
  str_src = nro_to_json(hash_src);
  str_dst = nro_to_json(hash_dst);
  tlib_pass_if_true("test_nro_hairy_utf8_object_json",
                    0 == nr_strcmp(str_src, str_dst), "str_src=%s str_dst=%s",
                    str_src, str_dst);

  expect_json[0] = 0;
  lg = nr_strlen(expect_json);
  snprintf(expect_json + lg, sizeof(expect_json) - lg,
           "{"
           "\"array\":["
           "0,"
           "0"
           "],"
           "\"hash\":{");

  sep = "";
  for (i = 0; i < N; i++) {
    int j;
    char buf_key[1024];
    char buf_val[4 * 1024];

    snprintf(buf_key, sizeof(buf_key), "%d", i);
    buf_val[0] = 0;
    for (j = 0; j < i; j++) {
      int lg1 = nr_strlen(buf_val);
      snprintf(buf_val + lg1, sizeof(buf_val) - lg1, "%s", "\\ud83d\\ude02");
    }
    lg = nr_strlen(expect_json);
    snprintf(expect_json + lg, sizeof(expect_json) - lg, "%s\"%s\":\"%s\"", sep,
             buf_key, buf_val);
    sep = ",";
  }

  lg = nr_strlen(expect_json);
  snprintf(expect_json + lg, sizeof(expect_json) - lg,
           "}"
           "}");

  tlib_pass_if_true("test_nro_hairy_utf8_object_json copy/json",
                    0 == nr_stricmp(expect_json, str_src),
                    "expect_json=>\n%s\nstr_src=>\n%s", expect_json, str_src);

  nro_delete(hash_dst);
  nro_delete(hash_src);
  nr_free(str_src);
  nr_free(str_dst);
}

/*
 * Tests with data known to cause issues.
 *
 * It isn't clear where this bogus data comes from, but the json encoder should
 * not loop when converting corner cases and bogus UTF8.
 */
static void test_nro_hairy_mangled_object_json(void) {
  nrobj_t* hash_src;
  nrobj_t* hash_dst;
  char* str_src;
  char* str_dst;
  char expect_json[1024];
  int lg;

  hash_src = nro_new_hash();
  nro_set_hash_string(hash_src, "index", "Database/\020\332)0\377\177/insert");

  hash_dst = nro_copy(hash_src);
  str_src = nro_to_json(hash_src);
  str_dst = nro_to_json(hash_dst);
  tlib_pass_if_true("test_nro_hairy_mangled_object_json",
                    0 == nr_strcmp(str_src, str_dst), "str_src=%s str_dst=%s",
                    str_src, str_dst);

  expect_json[0] = 0;
  lg = nr_strlen(expect_json);
  snprintf(expect_json + lg, sizeof(expect_json) - lg, "{");

  lg = nr_strlen(expect_json);
  snprintf(
      expect_json + lg, sizeof(expect_json) - lg,
      /*
       * That's right, the forward solidus (forward slash, eg '/') gets escaped
       */
      "\"index\":\"Database\\/\\u0010\\u00da)0\\u00ff\\u007f\\/insert\"");

  lg = nr_strlen(expect_json);
  snprintf(expect_json + lg, sizeof(expect_json) - lg, "}");

  tlib_pass_if_true("test_nro_hairy_mangled_object_json copy/json",
                    0 == nr_stricmp(expect_json, str_src),
                    "expect_json=>\n%s\nstr_src=>\n%s", expect_json, str_src);

  nro_delete(hash_dst);
  nro_delete(hash_src);
  nr_free(str_src);
  nr_free(str_dst);
}

static void test_nro_json_corner_cases(void) {
  nrobj_t* obj;
  nrotype_t t;
  nr_status_t err;
  int size;
  int v;
  int64_t l;
  double dv;

  obj = nro_create_from_json(" \t\f\r\n17"); /* tests space skipping */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_OBJECT_INT == t, "t=%d",
                    (int)t);
  v = nro_get_int(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_SUCCESS == err, "err=%d",
                    (int)err);
  tlib_pass_if_true("test_nro_json_corner_cases", 17 == v, "v=%d", v);
  nro_delete(obj);

  obj = nro_create_from_json("bogus");
  tlib_pass_if_true("test_nro_json_corner_cases", 0 == obj, "obj=%p", obj);

  obj = nro_create_from_json("null"); /* tests space skipping */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_OBJECT_NONE == t, "t=%d",
                    (int)t);
  nro_delete(obj);

  obj = nro_create_from_json("false"); /* tests space skipping */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_OBJECT_BOOLEAN == t,
                    "t=%d", (int)t);
  v = nro_get_boolean(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_SUCCESS == err, "err=%d",
                    (int)err);
  tlib_pass_if_true("test_nro_json_corner_cases", 0 == v, "v=%d", v);
  nro_delete(obj);

  obj = nro_create_from_json("true");
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_OBJECT_BOOLEAN == t,
                    "t=%d", (int)t);
  v = nro_get_boolean(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases", NR_SUCCESS == err, "err=%d",
                    (int)err);
  tlib_pass_if_true("test_nro_json_corner_cases", 1 == v, "v=%d", v);
  nro_delete(obj);

  /* json has edge cases, part N+1: you can't give floats starting with '.' */

  obj = nro_create_from_json("1.0e100"); /* in range for a double */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases 1.0e100", NR_OBJECT_DOUBLE == t,
                    "t=%d", (int)t);
  dv = nro_get_double(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases 1.0e100", NR_SUCCESS == err,
                    "err=%d", (int)err);
  tlib_pass_if_true("test_nro_json_corner_cases 1.0e100", 1.0e100 == dv,
                    "dv=%f", dv);
  nro_delete(obj);

  obj = nro_create_from_json("7e3"); /* in range for a double */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases 7e3", NR_OBJECT_DOUBLE == t,
                    "t=%d", (int)t);
  dv = nro_get_double(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases 7e3", NR_SUCCESS == err,
                    "err=%d", (int)err);
  if (NR_SUCCESS == err) {
    tlib_pass_if_true("test_nro_json_corner_cases 7e3", 7000.0 == dv, "dv=%f",
                      dv);
  }
  nro_delete(obj);

  obj = nro_create_from_json("7e+03"); /* in range for a double */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases 7e+03", NR_OBJECT_DOUBLE == t,
                    "t=%d", (int)t);
  dv = nro_get_double(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases 7e+03", NR_SUCCESS == err,
                    "err=%d", (int)err);
  if (NR_SUCCESS == err) {
    tlib_pass_if_true("test_nro_json_corner_cases 7e+03", 7000.0 == dv, "dv=%f",
                      dv);
  }
  nro_delete(obj);

  obj = nro_create_from_json("-1.0e500"); /* out of range for a double */
  tlib_pass_if_null("out of range double", obj);

  obj = nro_create_from_json(
      "1000000000000000000000000"); /* exceeds LLONG_MAX */
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases >LLONG_MAX",
                    NR_OBJECT_LONG == t, "t=%d", (int)t);
  l = nro_get_long(obj, &err);
  tlib_pass_if_true("test_nro_json_corner_cases >LLONG_MAX", NR_SUCCESS == err,
                    "err=%d", (int)err);
  if (NR_SUCCESS == err) {
    tlib_pass_if_true("test_nro_json_corner_cases >LLONG_MAX", LLONG_MAX == l,
                      "l=" NR_INT64_FMT, l);
  }
  nro_delete(obj);

  obj = nro_create_from_json(" [  ]  ");
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases empty array",
                    NR_OBJECT_ARRAY == t, "t=%d", (int)t);
  size = nro_getsize(obj);
  tlib_pass_if_true("test_nro_json_corner_cases empty array", 0 == size,
                    "size=%d", size);
  nro_delete(obj);

  obj = nro_create_from_json(" [ 1  ]  ");
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases array lg=1",
                    NR_OBJECT_ARRAY == t, "t=%d", (int)t);
  size = nro_getsize(obj);
  tlib_pass_if_true("test_nro_json_corner_cases array lg=1", 1 == size,
                    "size=%d", size);
  nro_delete(obj);

  obj = nro_create_from_json(
      " [ 1 ,  ]  "); /* can't use , as a terminator in json */
  tlib_pass_if_true("test_nro_json_corner_cases array lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(" [ 1 ;  ]  "); /* malformed */
  tlib_pass_if_true("test_nro_json_corner_cases array lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(" {  }  ");
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases empty hash",
                    NR_OBJECT_HASH == t, "t=%d", (int)t);
  size = nro_getsize(obj);
  tlib_pass_if_true("test_nro_json_corner_cases empty hash", 0 == size,
                    "size=%d", size);
  nro_delete(obj);

  obj = nro_create_from_json(" { \"foo\" : 17  }  ");
  t = nro_type(obj);
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", NR_OBJECT_HASH == t,
                    "t=%d", (int)t);
  size = nro_getsize(obj);
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 1 == size,
                    "size=%d", size);
  nro_delete(obj);

  obj = nro_create_from_json(" { \"foo\" ; 17  }  "); /* use ; instead of : */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(" { 1 : 1  }  "); /* can't use non-string as key */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(
      " { \"foo\" : 1 ,  }  "); /* can't use , as a terminator */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(" { \"foo\" : AAA }  "); /* illegal value */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(
      " { \"foo\" : 1 ; \"bar\" : 2  }  "); /* can't use non , as a separator */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(
      " { \"foo\" : 1 , \"bar\" ; 2  }  "); /* must use : to separate key from
                                               value */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);

  obj = nro_create_from_json(
      " { \"foo\" : 1 , \"bar\" : AA  }  "); /* bad second value */
  tlib_pass_if_true("test_nro_json_corner_cases hash lg=1", 0 == obj, "obj=%p",
                    obj);
}

static void test_nro_mangled_json(void) {
  nrobj_t* obj;

  obj = nro_create_from_json("[[[");
  tlib_pass_if_true("test_nro_mangled_json", NULL == obj, "obj=%p", obj);

  obj = nro_create_from_json("]]]");
  tlib_pass_if_true("test_nro_mangled_json", NULL == obj, "obj=%p", obj);

  obj = nro_create_from_json("{{{");
  tlib_pass_if_true("test_nro_mangled_json", NULL == obj, "obj=%p", obj);

  obj = nro_create_from_json("}}}");
  tlib_pass_if_true("test_nro_mangled_json", NULL == obj, "obj=%p", obj);

  tlib_pass_if_null("single quote", nro_create_from_json("\""));
  tlib_pass_if_null("odd quotes", nro_create_from_json("\"\"\""));
}

static void test_basic_creation(void) {
  nrobj_t* ob;
  char* dump;

  ob = nro_new(NR_OBJECT_INVALID);
  tlib_pass_if_true("invalid nro_new", 0 == ob, "ob=%p", ob);

  nro_test_new(nro_new(NR_OBJECT_NONE),
               "\
Object Dump (0):\n\
  NONE\n");

  nro_test_new(nro_new(NR_OBJECT_BOOLEAN),
               "\
Object Dump (1):\n\
  BOOLEAN: 0\n");

  nro_test_new(nro_new(NR_OBJECT_INT),
               "\
Object Dump (4):\n\
  INT: 0\n");

  nro_test_new(nro_new(NR_OBJECT_LONG),
               "\
Object Dump (5):\n\
  LONG: 0\n");

  nro_test_new(nro_new(NR_OBJECT_ULONG),
               "\
Object Dump (6):\n\
  ULONG: 0\n");

  nro_test_new(nro_new(NR_OBJECT_DOUBLE),
               "\
Object Dump (7):\n\
  DOUBLE: 0.000000\n");

  nro_test_new(nro_new(NR_OBJECT_STRING),
               "\
Object Dump (8):\n\
  STRING: >>>(NULL)<<<\n");

  nro_test_new(nro_new(NR_OBJECT_JSTRING),
               "\
Object Dump (9):\n\
  JSTRING: >>>(NULL)<<<\n");

  nro_test_new(nro_new_hash(),
               "\
Object Dump (10):\n\
  HASH: size=0 allocated=8\n");

  nro_test_new(nro_new_array(),
               "\
Object Dump (11):\n\
  ARRAY: size=0 allocated=8\n");
}

static void test_create_objects(void) {
  nrobj_t* ob;
  char* dump;

  nro_test_new(nro_new_none(),
               "\
Object Dump (0):\n\
  NONE\n");

  nro_test_new(nro_new_boolean(1),
               "\
Object Dump (1):\n\
  BOOLEAN: 1\n");

  nro_test_new(nro_new_int(4),
               "\
Object Dump (4):\n\
  INT: 4\n");

  nro_test_new(nro_new_long((int64_t)5),
               "\
Object Dump (5):\n\
  LONG: 5\n");

  nro_test_new(nro_new_ulong((uint64_t)6),
               "\
Object Dump (6):\n\
  ULONG: 6\n");

  nro_test_new(nro_new_double(7.0),
               "\
Object Dump (7):\n\
  DOUBLE: 7.000000\n");

  nro_test_new(nro_new_string("hello"),
               "\
Object Dump (8):\n\
  STRING: >>>hello<<<\n");

  nro_test_new(nro_new_jstring("[1,2,3]"),
               "\
Object Dump (9):\n\
  JSTRING: >>>[1,2,3]<<<\n");

  nro_test_new(nro_new_hash(),
               "\
Object Dump (10):\n\
  HASH: size=0 allocated=8\n");

  nro_test_new(nro_new_array(),
               "\
Object Dump (11):\n\
  ARRAY: size=0 allocated=8\n");
}

/*
 * Now come a set of tests for each object type. The purpose of these tests
 * is to ensure that each object type behaves correctly, especially at the
 * "corners" where values can overflow or be misinterpreted by the code.
 * We check to make sure they can be asserted, set in arrays, set in hashes,
 * and converted. We also use the macros to create each data type as a means
 * of testing those macros.
 */

static void test_object_boolean(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  int size;
  int rv;
  nr_status_t err;
  int i;
  char* dump;
  nrotype_t t;

  ob = nro_new_boolean(1);
  nro_test("nro_new_boolean (1)", ob,
           "\
Object Dump (1):\n\
  BOOLEAN: 1\n");

  size = nro_getsize(ob);
  tlib_pass_if_true("nro_getsize fails on boolean", -1 == size, "size=%d",
                    size);

  rv = nro_get_boolean(ob, &err);
  tlib_pass_if_true("get boolean succeeds", (1 == rv) && (NR_SUCCESS == err),
                    "rv=%d err=%d", rv, (int)err);

  rv = nro_get_boolean(ob, 0);
  tlib_pass_if_true("get boolean without errp", 1 == rv, "rv=%d", rv);

  t = nro_type(ob);
  tlib_pass_if_true("boolean object type", NR_OBJECT_BOOLEAN == t, "t=%d", t);

  tob = nro_assert(ob, NR_OBJECT_BOOLEAN);
  tlib_pass_if_true("boolean object assert", ob == tob, "ob=%p tob=%p", ob,
                    tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_BOOLEAN != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong boolean object assert", 0 == tob, "tob=%p", tob);
    }
  }

  nro_delete(ob);

  ob = nro_new_boolean(1);
  tlib_pass_if_true("new boolean true (1)", 0 != ob, "ob=%p", ob);
  rv = nro_get_boolean(ob, &err);
  tlib_pass_if_true("new boolean true (1)", (1 == rv) && (NR_SUCCESS == err),
                    "rv=%d err=%d", rv, (int)err);
  nro_delete(ob);

  ob = nro_new_boolean(-1);
  tlib_pass_if_true("new boolean true (-1)", 0 != ob, "ob=%p", ob);
  rv = nro_get_boolean(ob, &err);
  tlib_pass_if_true("new boolean true (-1)", (1 == rv) && (NR_SUCCESS == err),
                    "rv=%d err=%d", rv, (int)err);
  nro_delete(ob);

  ob = nro_new_boolean(0);
  tlib_pass_if_true("new boolean false", 0 != ob, "ob=%p", ob);
  rv = nro_get_boolean(ob, &err);
  tlib_pass_if_true("new boolean false", (0 == rv) && (NR_SUCCESS == err),
                    "rv=%d err=%d", rv, (int)err);
  nro_delete(ob);

  hash = nro_new_hash();
  nro_set_hash_boolean(hash, "abc", 1);
  nro_test("nro_set_hash_boolean (hash, \"abc\", 1)", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    BOOLEAN: 1\n\
  }\n");

  rv = nro_get_hash_boolean(hash, "abc", &err);
  tlib_pass_if_true("get hash boolean succeeds",
                    (1 == rv) && (NR_SUCCESS == err), "rv=%d err=%d", rv,
                    (int)err);
  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_true("get hash int fails", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  array = nro_new_array();
  nro_set_array_boolean(array, 0, 1);
  nro_test("nro_set_array_boolean (array, 0, 1)", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    BOOLEAN: 1\n\
  }\n");

  rv = nro_get_array_boolean(array, 1, &err);
  tlib_pass_if_true("get array boolean succeeds",
                    (1 == rv) && (NR_SUCCESS == err), "rv=%d err=%d", rv,
                    (int)err);
  rv = nro_get_array_boolean(NULL, 1, &err);
  tlib_pass_if_true("get array boolean fails OK",
                    (-1 == rv) && (NR_FAILURE == err), "rv=%d err=%d", rv,
                    (int)err);
  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_true("get array int fails", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  nro_delete(array);
  nro_delete(hash);
}

/*
 * This test has additional tests over the previous two integral types as the
 * values are stored internally as ints, and we must check for overflow and
 * underflow. Also check we can store values greater than and less than the
 * maximum and minimum shorts.
 */
static void test_object_int(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  int size;
  int rv;
  int64_t lv;
  nr_status_t err;
  int i;
  char* dump;

  ob = nro_new_int(123);
  nro_test("nro_new_int (123)", ob,
           "\
Object Dump (4):\n\
  INT: 123\n");

  size = nro_getsize(ob);
  tlib_pass_if_true("nro_getsize fails on int", -1 == size, "size=%d", size);

  rv = nro_get_int(ob, &err);
  tlib_pass_if_true("get int succeeds", (123 == rv) && (NR_SUCCESS == err),
                    "rv=%d err=%d", rv, (int)err);

  rv = nro_get_int(ob, 0);
  tlib_pass_if_true("get int without errp", 123 == rv, "rv=%d", rv);

  rv = nro_type(ob);
  tlib_pass_if_true("int object type", NR_OBJECT_INT == rv, "rv=%d", rv);

  tob = nro_assert(ob, NR_OBJECT_INT);
  tlib_pass_if_true("int object assert", ob == tob, "ob=%p tob=%p", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_INT != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong int object assert", 0 == tob, "tob=%p", tob);
    }
  }

  INT_TEST(-1, -1, );
  INT_TEST(UINT_MAX, UINT_MAX, (unsigned int));
  INT_TEST(INT_MAX, INT_MAX, );
  INT_TEST((unsigned int)INT_MAX + 1, (unsigned int)INT_MAX + 1,
           (unsigned int));
  INT_TEST(INT_MIN, INT_MIN, );
  INT_TEST(UINT_MAX + 1, 0, );

  INT_TEST(CHAR_MAX, CHAR_MAX, );
  INT_TEST(CHAR_MIN, CHAR_MIN, );
  INT_TEST(UCHAR_MAX, UCHAR_MAX, );
  INT_TEST(UCHAR_MAX + 1, UCHAR_MAX + 1, );
  INT_TEST(CHAR_MIN - 1, CHAR_MIN - 1, );
  INT_TEST(SHRT_MAX, SHRT_MAX, );
  INT_TEST(SHRT_MIN, SHRT_MIN, );
  INT_TEST(USHRT_MAX, USHRT_MAX, );
  INT_TEST(USHRT_MAX + 1, USHRT_MAX + 1, );
  INT_TEST(SHRT_MIN - 1, SHRT_MIN - 1, );

  hash = nro_new_hash();
  nro_set_hash_int(hash, "abc", 123);
  nro_test("nro_set_hash_int (hash, \"abc\", 123)", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    INT: 123\n\
  }\n");

  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_true("get hash int succeeds", (123 == rv) && (NR_SUCCESS == err),
                    "rv=%d err=%d", rv, (int)err);
  lv = nro_get_hash_long(hash, "abc", &err);
  tlib_pass_if_true("get hash long succeeds for ints",
                    (123LL == lv) && (NR_SUCCESS == err), "lv=%d err=%d",
                    (int)lv, (int)err);

  array = nro_new_array();
  nro_set_array_int(array, 0, 123);
  nro_test("nro_set_array_int (array, 0, 123)", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    INT: 123\n\
  }\n");

  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_true("get array int succeeds",
                    (123 == rv) && (NR_SUCCESS == err), "rv=%d err=%d", rv,
                    (int)err);
  rv = nro_get_array_int(NULL, 1, &err);
  tlib_pass_if_true("get array int fails OK", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);
  lv = nro_get_array_long(array, 1, &err);
  tlib_pass_if_true("get array long succeeds",
                    (123LL == lv) && (NR_SUCCESS == err), "lv=%d err=%d",
                    (int)lv, (int)err);

  nro_delete(array);
  nro_delete(hash);
  nro_delete(ob);
}

/*
 * This test also has additional tests for overflow / underflow.
 */
static void test_object_long(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  nrobj_t* json;

  int rv;
  int size;
  nr_status_t err;
  int i;
  int64_t lv;
  char* dump;

  ob = nro_new_long(123);
  nro_test("nro_new_long (123)", ob,
           "\
Object Dump (5):\n\
  LONG: 123\n");

  size = nro_getsize(ob);
  tlib_pass_if_true("nro_getsize fails on long", -1 == size, "size=%d", size);

  lv = nro_get_long(ob, &err);
  tlib_pass_if_true("get long succeeds", (123 == lv) && (NR_SUCCESS == err),
                    "lv=" NR_INT64_FMT " err=%d", lv, (int)err);

  lv = nro_get_long(ob, 0);
  tlib_pass_if_true("get long without errp", 123 == lv, "lv=" NR_INT64_FMT "",
                    lv);

  rv = nro_type(ob);
  tlib_pass_if_true("long object type", NR_OBJECT_LONG == rv, "rv=%d", rv);

  tob = nro_assert(ob, NR_OBJECT_LONG);
  tlib_pass_if_true("long object assert", ob == tob, "ob=%p tob=%p", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_LONG != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong long object assert", 0 == tob, "tob=%p", tob);
    }
  }

  LONG_TEST(-1, -1, );
  LONG_TEST(ULLONG_MAX, ULLONG_MAX, (uint64_t));
  LONG_TEST(LLONG_MAX, LLONG_MAX, );
  LONG_TEST((uint64_t)LLONG_MAX + 1, (uint64_t)LLONG_MAX + 1, (uint64_t));
  LONG_TEST(LLONG_MIN, LLONG_MIN, );
  LONG_TEST(ULLONG_MAX + 1, 0, );

  LONG_TEST(CHAR_MAX, CHAR_MAX, );
  LONG_TEST(CHAR_MIN, CHAR_MIN, );
  LONG_TEST(UCHAR_MAX, UCHAR_MAX, );
  LONG_TEST(UCHAR_MAX + 1, UCHAR_MAX + 1, );
  LONG_TEST(CHAR_MIN - 1, CHAR_MIN - 1, );
  LONG_TEST(SHRT_MAX, SHRT_MAX, );
  LONG_TEST(SHRT_MIN, SHRT_MIN, );
  LONG_TEST(USHRT_MAX, USHRT_MAX, );
  LONG_TEST(SHRT_MIN - 1, SHRT_MIN - 1, );
  LONG_TEST(INT_MAX, INT_MAX, );
  LONG_TEST(INT_MIN, INT_MIN, );
  LONG_TEST(UINT_MAX, UINT_MAX, );
  LONG_TEST((int64_t)UINT_MAX + 1, (int64_t)UINT_MAX + 1, );
  LONG_TEST((int64_t)INT_MIN - 1, (int64_t)INT_MIN - 1, );

  hash = nro_new_hash();
  nro_set_hash_long(hash, "abc", 123);
  nro_test("nro_set_hash_long (hash, \"abc\", 123)", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    LONG: 123\n\
  }\n");

  lv = nro_get_hash_long(hash, "abc", &err);
  tlib_pass_if_true("get hash long succeeds",
                    (123 == lv) && (NR_SUCCESS == err),
                    "lv=" NR_INT64_FMT " err=%d", lv, (int)err);
  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_true("get hash int fails", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  array = nro_new_array();
  nro_set_array_long(array, 0, 123);
  nro_test("nro_set_array_long (array, 0, 123)", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    LONG: 123\n\
  }\n");

  lv = nro_get_array_long(array, 1, &err);
  tlib_pass_if_true("get array long succeeds",
                    (123 == lv) && (NR_SUCCESS == err),
                    "lv=" NR_INT64_FMT " err=%d", lv, (int)err);
  lv = nro_get_array_long(NULL, 1, &err);
  tlib_pass_if_true("get array long fails OK",
                    (-1LL == lv) && (NR_FAILURE == err), "lv=%d err=%d",
                    (int)lv, (int)err);
  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_true("get array int fails", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  nro_delete(hash);
  hash = nro_new_hash();
  nro_set_hash_long(hash, "ti", 1482959525577);
  lv = 1482959525577;
  tlib_pass_if_long_equal("get double passes", lv,
                          nro_get_hash_long(hash, "ti", NULL));

  json = nro_create_from_json("{ \"ti\": 1482959525577 }");
  lv = 1482959525577;
  tlib_pass_if_long_equal("get double passes", lv,
                          nro_get_hash_long(json, "ti", NULL));

  nro_delete(array);
  nro_delete(hash);
  nro_delete(ob);
  nro_delete(json);
}

/*
 * This test also has additional tests for overflow / underflow.
 */
static void test_object_ulong(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;

  int rv;
  int size;
  nr_status_t err;
  int i;
  uint64_t ulv;
  char* dump;

  ob = nro_new_ulong(123);
  nro_test("nro_new_ulong (123)", ob,
           "\
Object Dump (6):\n\
  ULONG: 123\n");

  size = nro_getsize(ob);
  tlib_pass_if_int_equal("nro_getsize fails on ulong", -1, size);

  ulv = nro_get_ulong(ob, &err);
  tlib_pass_if_uint64_t_equal("get long succeeds", 123, ulv);
  tlib_pass_if_status_success("get long succeeds", err);

  ulv = nro_get_ulong(ob, NULL);
  tlib_pass_if_uint64_t_equal("get long without errp", 123, ulv);

  rv = nro_type(ob);
  tlib_pass_if_int_equal("ulong object type", (int)NR_OBJECT_ULONG, (int)rv);

  tob = nro_assert(ob, NR_OBJECT_ULONG);
  tlib_pass_if_ptr_equal("long object assert", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_ULONG != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_fail_if_ptr_equal("wrong long object assert", ob, tob);
    }
  }

  ULONG_TEST(0, 0, );
  ULONG_TEST(ULLONG_MAX, ULLONG_MAX, (uint64_t));
  ULONG_TEST(LLONG_MAX, LLONG_MAX, );
  ULONG_TEST((uint64_t)LLONG_MAX + 1, (uint64_t)LLONG_MAX + 1, (uint64_t));
  ULONG_TEST(ULLONG_MAX + 1, 0, );

  ULONG_TEST(UCHAR_MAX, UCHAR_MAX, );
  ULONG_TEST(UCHAR_MAX + 1, UCHAR_MAX + 1, );
  ULONG_TEST(USHRT_MAX, USHRT_MAX, );
  ULONG_TEST(UINT_MAX, UINT_MAX, );
  ULONG_TEST((int64_t)UINT_MAX + 1, (int64_t)UINT_MAX + 1, );

  hash = nro_new_hash();
  nro_set_hash_ulong(hash, "abc", 123);
  nro_test("nro_set_hash_ulong (hash, \"abc\", 123)", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    ULONG: 123\n\
  }\n");

  ulv = nro_get_hash_ulong(hash, "abc", &err);
  tlib_pass_if_uint64_t_equal("get hash long succeeds", 123, ulv);
  tlib_pass_if_status_success("get hash long succeeds", err);

  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_int_equal("get hash int fails", -1, rv);
  tlib_fail_if_status_success("get hash int fails", err);

  array = nro_new_array();
  nro_set_array_ulong(array, 0, 123);
  nro_test("nro_set_array_ulong (array, 0, 123)", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    ULONG: 123\n\
  }\n");

  ulv = nro_get_array_ulong(array, 1, &err);
  tlib_pass_if_uint64_t_equal("get array long succeeds", 123, ulv);
  tlib_pass_if_status_success("get array long succeeds", err);

  ulv = nro_get_array_ulong(NULL, 1, &err);
  tlib_pass_if_uint64_t_equal("get array long fails OK", 0, ulv);
  tlib_fail_if_status_success("get array long fails OK", err);

  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_int_equal("get array int fails", -1, rv);
  tlib_fail_if_status_success("get array int fails", err);

  nro_delete(hash);
  nro_delete(array);
  nro_delete(ob);
}

/*
 * This test also has additional tests for overflow / underflow.
 */
static void test_object_double(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  int rv;
  nr_status_t err;
  int size;
  int i;
  double dv;
  char* dump;
  nrotype_t t;

  ob = nro_new_double(123.456);
  nro_test("nro_new_double (123.456)", ob,
           "\
Object Dump (7):\n\
  DOUBLE: 123.456000\n");

  size = nro_getsize(ob);
  tlib_pass_if_true("nro_getsize fails on double", -1 == size, "size=%d", size);

  dv = nro_get_double(ob, &err);
  tlib_pass_if_true("get double succeeds",
                    (123.456 == dv) && (NR_SUCCESS == err), "dv=%lf err=%d", dv,
                    (int)err);

  dv = nro_get_double(ob, 0);
  tlib_pass_if_true("get double without errp", 123.456 == dv, "dv=%lf", dv);

  t = nro_type(ob);
  tlib_pass_if_true("double object type", NR_OBJECT_DOUBLE == t, "t=%d", t);

  tob = nro_assert(ob, NR_OBJECT_DOUBLE);
  tlib_pass_if_true("double object assert", ob == tob, "ob=%p tob=%p", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_DOUBLE != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong double object assert", 0 == tob, "tob=%p", tob);
    }
  }

  DOUBLE_TEST(-1.2, -1.2, );
  DOUBLE_TEST(DBL_MAX, DBL_MAX, );
  DOUBLE_TEST(DBL_MIN, DBL_MIN, );
  DOUBLE_TEST(ULLONG_MAX, ULLONG_MAX, );
  DOUBLE_TEST(LLONG_MAX, LLONG_MAX, );
#if !(defined(__sun__) || defined(__sun)) /* solaris numerics differ from \
                                             linux or macosx in 1 ulp. */
  DOUBLE_TEST((double)LLONG_MAX + 1, (double)LLONG_MAX + 1, (uint64_t));
#endif
  DOUBLE_TEST(LLONG_MIN, LLONG_MIN, );
  DOUBLE_TEST((double)ULLONG_MAX + 1, (double)ULLONG_MAX + 1, );
#if !(defined(__sun__) || defined(__sun)) /* solaris numerics differ from \
                                             linux or macosx in 1 ulp. */
  DOUBLE_TEST((double)LLONG_MIN - 1, (double)LLONG_MIN - 1, );
#endif
  DOUBLE_TEST(CHAR_MAX, CHAR_MAX, );
  DOUBLE_TEST(CHAR_MIN, CHAR_MIN, );
  DOUBLE_TEST(SHRT_MAX, SHRT_MAX, );
  DOUBLE_TEST(SHRT_MIN, SHRT_MIN, );
  DOUBLE_TEST(USHRT_MAX, USHRT_MAX, );
  DOUBLE_TEST(USHRT_MAX + 1, USHRT_MAX + 1, );
  DOUBLE_TEST(SHRT_MIN - 1, SHRT_MIN - 1, );
  DOUBLE_TEST(INT_MAX, INT_MAX, );
  DOUBLE_TEST(INT_MIN, INT_MIN, );
  DOUBLE_TEST(UINT_MAX, UINT_MAX, );
  DOUBLE_TEST((double)UINT_MAX + 1, (double)UINT_MAX + 1, );
  DOUBLE_TEST((double)INT_MIN - 1, (double)INT_MIN - 1, );

  hash = nro_new_hash();
  nro_set_hash_double(hash, "abc", 123.456);
  nro_test("nro_set_hash_double (hash, \"abc\", 123.456)", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    DOUBLE: 123.456000\n\
  }\n");

  dv = nro_get_hash_double(hash, "abc", &err);
  tlib_pass_if_true("get hash double succeeds",
                    (123.456 == dv) && (NR_SUCCESS == err), "dv=%lf err=%d", dv,
                    (int)err);
  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_true("get hash int fails", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  array = nro_new_array();
  nro_set_array_double(array, 0, 123.456);
  nro_test("nro_set_array_double (array, 0, 123.456)", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    DOUBLE: 123.456000\n\
  }\n");

  dv = nro_get_array_double(array, 1, &err);
  tlib_pass_if_true("get array double succeeds",
                    (123.456 == dv) && (NR_SUCCESS == err), "dv=%lf err=%d", dv,
                    (int)err);
  dv = nro_get_array_double(NULL, 1, &err);
  tlib_pass_if_true("get array double fails OK",
                    (-1.0 == dv) && (NR_FAILURE == err), "dv=%lf err=%d", dv,
                    (int)err);
  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_true("get array int fails", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  nro_delete(array);
  nro_delete(hash);
  nro_delete(ob);
}

/*
 * Not a great deal to test specifically for this type, we just need to check
 * the corner case where NULL is passed and how it is dealt with.
 */
static void test_object_string(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  int rv;
  nr_status_t err;
  int i;
  int size;
  const char* sv;
  const char* s;
  char* dump;
  nrotype_t t;

  s = "123";
  ob = nro_new_string(s);
  nro_test("nro_new_string (s)", ob,
           "\
Object Dump (8):\n\
  STRING: >>>123<<<\n");

  size = nro_getsize(ob);
  tlib_pass_if_true("nro_getsize fails on string", -1 == size, "size=%d", size);

  sv = nro_get_string(ob, &err);
  tlib_pass_if_true("get string succeeds",
                    (0 == nr_strcmp(sv, "123")) && (NR_SUCCESS == err),
                    "sv=%s err=%d", sv ? sv : "(NULL)", (int)err);

  sv = nro_get_string(ob, 0);
  tlib_pass_if_true("get string without errp", 0 == nr_strcmp(sv, "123"),
                    "sv=%s", sv ? sv : "(NULL)");

  tlib_pass_if_true("new string object dups string", s != sv, "s=%s sv=%s",
                    s ? s : "(NULL)", sv ? sv : "(NULL)");
  nro_delete(ob);

  ob = nro_new_string(0);
  tlib_pass_if_true("new string NULL succeeds", 0 != ob, "ob=%p", ob);
  sv = nro_get_string(ob, &err);
  tlib_pass_if_true("get NULL string returns empty string",
                    (NR_SUCCESS == err) && (0 == nr_strcmp(sv, "")),
                    "err=%d sv=%s", (int)err, sv ? sv : "(NULL)");
  nro_test("new string NULL", ob,
           "\
Object Dump (8):\n\
  STRING: >>><<<\n");

  t = nro_type(ob);
  tlib_pass_if_true("string object type", NR_OBJECT_STRING == t, "t=%d", t);

  tob = nro_assert(ob, NR_OBJECT_STRING);
  tlib_pass_if_true("string object assert", ob == tob, "ob=%p tob=%p", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_STRING != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong string object assert", 0 == tob, "tob=%p", tob);
    }
  }

  hash = nro_new_hash();
  nro_set_hash_string(hash, "abc", "123");
  nro_test("nro_set_hash_string (hash, \"abc\", \"123\")", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    STRING: >>>123<<<\n\
  }\n");

  sv = nro_get_hash_string(hash, "abc", &err);
  tlib_pass_if_true("get hash string",
                    (NR_SUCCESS == err) && (0 == nr_strcmp(sv, "123")),
                    "err=%d sv=%s", (int)err, sv ? sv : "(NULL)");
  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_true("incorrect get hash int", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  array = nro_new_array();
  nro_set_array_string(array, 0, "123");
  nro_test("nro_set_array_string (array, 0, \"123\")", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    STRING: >>>123<<<\n\
  }\n");

  sv = nro_get_array_string(array, 1, &err);
  tlib_pass_if_true("get array string",
                    (NR_SUCCESS == err) && (0 == nr_strcmp(sv, "123")),
                    "err=%d sv=%s", (int)err, sv ? sv : "(NULL)");
  sv = nro_get_array_string(NULL, 1, &err);
  tlib_pass_if_true("get array string fails OK",
                    (NULL == sv) && (NR_FAILURE == err), "err=%d sv=%s",
                    (int)err, sv ? sv : "(NULL)");
  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_true("incorrect get array int",
                    (-1 == rv) && (NR_FAILURE == err), "rv=%d err=%d", rv,
                    (int)err);

  nro_delete(array);
  nro_delete(hash);
  nro_delete(ob);
}

static void test_object_jstring(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  int rv;
  nr_status_t err;
  int i;
  int size;
  const char* sv;
  const char* s;
  char* dump;
  nrotype_t t;

  s = "[1,2,3]";
  ob = nro_new_jstring(s);
  nro_test("nro_new_jstring (s)", ob,
           "\
Object Dump (9):\n\
  JSTRING: >>>[1,2,3]<<<\n");

  size = nro_getsize(ob);
  tlib_pass_if_true("nro_getsize fails on jstring", -1 == size, "size=%d",
                    size);

  sv = nro_get_string(ob, &err);
  tlib_pass_if_true("get string fails jstring", 0 == sv, "sv=%p err=%d", sv,
                    (int)err);

  sv = nro_get_jstring(ob, &err);
  tlib_pass_if_true("get jstring succeeds jstring",
                    0 == nr_strcmp("[1,2,3]", sv), "sv=%s err=%d",
                    NRSAFESTR(sv), (int)err);

  sv = nro_get_jstring(ob, 0);
  tlib_pass_if_true("get jstring on jstring without errp",
                    0 == nr_strcmp(sv, "[1,2,3]"), "sv=%s", sv ? sv : "(NULL)");

  tlib_pass_if_true("new jstring object dups jstring", s != sv, "s=%s sv=%s",
                    s ? s : "(NULL)", sv ? sv : "(NULL)");
  nro_delete(ob);

  ob = nro_new_jstring(0);
  tlib_pass_if_true("new jstring NULL succeeds", 0 != ob, "ob=%p", ob);

  sv = nro_get_jstring(ob, &err);
  tlib_pass_if_true("get NULL jstring returns empty string",
                    (NR_SUCCESS == err) && (0 == nr_strcmp(sv, "")),
                    "err=%d sv=%s", (int)err, sv ? sv : "(NULL)");
  nro_test("new jstring NULL", ob,
           "\
Object Dump (9):\n\
  JSTRING: >>><<<\n");

  t = nro_type(ob);
  tlib_pass_if_true("jstring object type", NR_OBJECT_JSTRING == t, "t=%d", t);

  tob = nro_assert(ob, NR_OBJECT_JSTRING);
  tlib_pass_if_true("jstring object assert", ob == tob, "ob=%p tob=%p", ob,
                    tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_JSTRING != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong jstring object assert", 0 == tob, "tob=%p", tob);
    }
  }

  hash = nro_new_hash();
  nro_set_hash_jstring(hash, "abc", "123");
  nro_test("nro_set_hash_jstring (hash, \"abc\", \"123\")", hash,
           "\
Object Dump (10):\n\
  HASH: size=1 allocated=8\n\
  ['abc'] = {\n\
    JSTRING: >>>123<<<\n\
  }\n");

  sv = nro_get_hash_jstring(hash, "abc", &err);
  tlib_pass_if_true("get hash jstring",
                    (NR_SUCCESS == err) && (0 == nr_strcmp(sv, "123")),
                    "err=%d sv=%s", (int)err, sv ? sv : "(NULL)");
  rv = nro_get_hash_int(hash, "abc", &err);
  tlib_pass_if_true("incorrect get hash int", (-1 == rv) && (NR_FAILURE == err),
                    "rv=%d err=%d", rv, (int)err);

  array = nro_new_array();
  nro_set_array_jstring(array, 0, "123");
  nro_test("nro_set_array_jstring (array, 0, \"123\")", array,
           "\
Object Dump (11):\n\
  ARRAY: size=1 allocated=8\n\
  [1] = {\n\
    JSTRING: >>>123<<<\n\
  }\n");

  sv = nro_get_array_jstring(array, 1, &err);
  tlib_pass_if_true("get array jstring",
                    (NR_SUCCESS == err) && (0 == nr_strcmp(sv, "123")),
                    "err=%d sv=%s", (int)err, sv ? sv : "(NULL)");
  sv = nro_get_array_jstring(NULL, 1, &err);
  tlib_pass_if_true("get array jstring fails OK",
                    (NULL == sv) && (NR_FAILURE == err), "err=%d sv=%s",
                    (int)err, sv ? sv : "(NULL)");
  rv = nro_get_array_int(array, 1, &err);
  tlib_pass_if_true("incorrect get array int",
                    (-1 == rv) && (NR_FAILURE == err), "rv=%d err=%d", rv,
                    (int)err);

  nro_delete(array);
  nro_delete(hash);
  nro_delete(ob);
}

static void test_object_hash(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  nr_status_t setcode;
  int i;
  char* dump;
  char* js;

  ob = nro_new_hash();
  nro_test("new_new (NR_OBJECT_HASH)", ob,
           "\
Object Dump (10):\n\
  HASH: size=0 allocated=8\n");

  js = nro_to_json(ob);
  tlib_pass_if_true("new hash to json", (0 != js) && (0 == nr_strcmp(js, "{}")),
                    "js=%s", js);
  nr_free(js);

  tob = nro_assert(ob, NR_OBJECT_HASH);
  tlib_pass_if_true("hash object assert", ob == tob, "ob=%p tob=%p", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_HASH != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong hash object assert", 0 == tob, "tob=%p", tob);
    }
  }

  /* Verify using a NULL key gives an error. */
  setcode = nro_set_hash(ob, 0, ob);
  tlib_pass_if_true("NULL key hash set", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);

  /* And same with a NULL hash. */
  setcode = nro_set_hash(0, 0, 0);
  tlib_pass_if_true("NULL hash set", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);

  /* And with an empty string as a key. */
  setcode = nro_set_hash(ob, "", ob);
  tlib_pass_if_true("empty string key", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);

  /* Add one of each data type to the hash. */
  setcode = nro_set_hash_boolean(ob, "boolean", 1);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_int(ob, "int", 789);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_long(ob, "long", 101112);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_ulong(ob, "ulong", 101112);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_double(ob, "double0", 131415.1617);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_double(ob, "double1", 1.1111);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_string(ob, "string", "abc");
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_jstring(ob, "jstring", "[1,2,3]");
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  hash = nro_new_hash();
  array = nro_new_array();
  setcode = nro_set_hash_long(hash, "subhash-long", 1);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_int(hash, "subhash-int", 2);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_int(array, 0, 4);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_long(array, 0, 5);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash(ob, "hash", hash);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash(ob, "array", array);
  tlib_pass_if_true("hash set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  nro_delete(hash);
  nro_delete(array);

  nro_test("populated hash", ob,
           "\
Object Dump (10):\n\
  HASH: size=10 allocated=16\n\
  ['boolean'] = {\n\
    BOOLEAN: 1\n\
  }\n\
  ['int'] = {\n\
    INT: 789\n\
  }\n\
  ['long'] = {\n\
    LONG: 101112\n\
  }\n\
  ['ulong'] = {\n\
    ULONG: 101112\n\
  }\n\
  ['double0'] = {\n\
    DOUBLE: 131415.161700\n\
  }\n\
  ['double1'] = {\n\
    DOUBLE: 1.111100\n\
  }\n\
  ['string'] = {\n\
    STRING: >>>abc<<<\n\
  }\n\
  ['jstring'] = {\n\
    JSTRING: >>>[1,2,3]<<<\n\
  }\n\
  ['hash'] = {\n\
    HASH: size=2 allocated=2\n\
    ['subhash-long'] = {\n\
      LONG: 1\n\
    }\n\
    ['subhash-int'] = {\n\
      INT: 2\n\
    }\n\
  }\n\
  ['array'] = {\n\
    ARRAY: size=2 allocated=2\n\
    [1] = {\n\
      INT: 4\n\
    }\n\
    [2] = {\n\
      LONG: 5\n\
    }\n\
  }\n");

  js = nro_to_json(ob);
  tlib_pass_if_true("populated hash to json",
                    (0 != js)
                        && (0
                            == nr_strcmp(js,
                                         "\
{\"boolean\":true,\"int\":789,\"long\":101112,\"ulong\":101112,\"double0\":\
131415.16170,\"double1\":1.11110,\"string\":\"abc\",\"jstring\":[1,2,3],\"hash\":{\"subhash-long\":1,\
\"subhash-int\":2},\"array\":[4,5]}")),
                    "js=%s", js);

  tob = nro_create_from_json(js);
  nro_test("populated hash from JSON", tob,
           "\
Object Dump (10):\n\
  HASH: size=10 allocated=16\n\
  ['boolean'] = {\n\
    BOOLEAN: 1\n\
  }\n\
  ['int'] = {\n\
    INT: 789\n\
  }\n\
  ['long'] = {\n\
    INT: 101112\n\
  }\n\
  ['ulong'] = {\n\
    INT: 101112\n\
  }\n\
  ['double0'] = {\n\
    DOUBLE: 131415.161700\n\
  }\n\
  ['double1'] = {\n\
    DOUBLE: 1.111100\n\
  }\n\
  ['string'] = {\n\
    STRING: >>>abc<<<\n\
  }\n\
  ['jstring'] = {\n\
    ARRAY: size=3 allocated=8\n\
    [1] = {\n\
      INT: 1\n\
    }\n\
    [2] = {\n\
      INT: 2\n\
    }\n\
    [3] = {\n\
      INT: 3\n\
    }\n\
  }\n\
  ['hash'] = {\n\
    HASH: size=2 allocated=8\n\
    ['subhash-long'] = {\n\
      INT: 1\n\
    }\n\
    ['subhash-int'] = {\n\
      INT: 2\n\
    }\n\
  }\n\
  ['array'] = {\n\
    ARRAY: size=2 allocated=8\n\
    [1] = {\n\
      INT: 4\n\
    }\n\
    [2] = {\n\
      INT: 5\n\
    }\n\
  }\n");

  nro_delete(tob);
  nr_free(js);

  /*
   * Replace an existing element in the hash with another one.
   */
  setcode = nro_set_hash_long(ob, "long", 101112);
  tlib_pass_if_true("replace existing element", NR_SUCCESS == setcode,
                    "setcode=%d", (int)setcode);
  nro_delete(tob);

  nro_test("replaced hash", ob,
           "\
Object Dump (10):\n\
  HASH: size=10 allocated=16\n\
  ['boolean'] = {\n\
    BOOLEAN: 1\n\
  }\n\
  ['int'] = {\n\
    INT: 789\n\
  }\n\
  ['long'] = {\n\
    LONG: 101112\n\
  }\n\
  ['ulong'] = {\n\
    ULONG: 101112\n\
  }\n\
  ['double0'] = {\n\
    DOUBLE: 131415.161700\n\
  }\n\
  ['double1'] = {\n\
    DOUBLE: 1.111100\n\
  }\n\
  ['string'] = {\n\
    STRING: >>>abc<<<\n\
  }\n\
  ['jstring'] = {\n\
    JSTRING: >>>[1,2,3]<<<\n\
  }\n\
  ['hash'] = {\n\
    HASH: size=2 allocated=2\n\
    ['subhash-long'] = {\n\
      LONG: 1\n\
    }\n\
    ['subhash-int'] = {\n\
      INT: 2\n\
    }\n\
  }\n\
  ['array'] = {\n\
    ARRAY: size=2 allocated=2\n\
    [1] = {\n\
      INT: 4\n\
    }\n\
    [2] = {\n\
      LONG: 5\n\
    }\n\
  }\n");

  /*
   * Capacity testing
   */
  for (i = 0; i < 1024; i++) {
    char keyname[24];
    char keyval[64];
    snprintf(keyname, sizeof(keyname), "Hash Key Name #%04d", i);
    snprintf(keyval, sizeof(keyval),
             "Hash Key value #%04d - slightly longer string this time", i);
    setcode = nro_set_hash_string(ob, keyname, keyval);
    tlib_pass_if_true("hash capacity testing", NR_SUCCESS == setcode,
                      "setcode=%d", (int)setcode);
  }

  nro_delete(tob);
  nro_delete(ob);
}

static void test_object_array(void) {
  nrobj_t* ob;
  nrobj_t* tob;
  nrobj_t* hash;
  nrobj_t* array;
  nr_status_t setcode;
  int i;
  int size;
  char* dump;
  char* js;

  ob = nro_new_array();
  nro_test("empty array", ob,
           "\
Object Dump (11):\n\
  ARRAY: size=0 allocated=8\n");

  tob = nro_assert(ob, NR_OBJECT_ARRAY);
  tlib_pass_if_true("hash object assert", ob == tob, "ob=%p tob=%p", ob, tob);

  for (i = 0; i < num_otypes; i++) {
    if (NR_OBJECT_ARRAY != otypes[i]) {
      tob = nro_assert(ob, otypes[i]);
      tlib_pass_if_true("wrong hash object assert", 0 == tob, "tob=%p", tob);
    }
  }

  js = nro_to_json(ob);
  tlib_pass_if_true("new array to json",
                    (0 != js) && (0 == nr_strcmp(js, "[]")), "js=%s", js);
  nr_free(js);

  /*
   * Verify using out of bounds index gives an error.
   */
  setcode = nro_set_array(ob, -1, ob);
  tlib_pass_if_true("out of bounds index", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array(ob, 2, ob);
  tlib_pass_if_true("out of bounds index", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);

  /*
   * And same with a NULL array.
   */
  setcode = nro_set_array(0, 0, 0);
  tlib_pass_if_true("NULL array", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);

  /*
   * Add one of each data type to the array.
   */
  setcode = nro_set_array_boolean(ob, 0, 1);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_int(ob, 0, 123);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_int(ob, 0, 456);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_int(ob, 0, 789);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_long(ob, 0, 101112);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_ulong(ob, 0, 101112);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_double(ob, 0, 131415.1617);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_string(ob, 0, "abc");
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_jstring(ob, 0, "[1,2,3]");
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  hash = nro_new_hash();
  array = nro_new_array();
  setcode = nro_set_hash_long(hash, "subhash-long", 1);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_hash_int(hash, "subhash-int", 2);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_int(array, 0, 4);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_long(array, 0, 5);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array(ob, 0, hash);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array(ob, 0, array);
  tlib_pass_if_true("array set", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  nro_delete(hash);
  nro_delete(array);

  nro_test("populated array", ob,
           "\
Object Dump (11):\n\
  ARRAY: size=11 allocated=16\n\
  [1] = {\n\
    BOOLEAN: 1\n\
  }\n\
  [2] = {\n\
    INT: 123\n\
  }\n\
  [3] = {\n\
    INT: 456\n\
  }\n\
  [4] = {\n\
    INT: 789\n\
  }\n\
  [5] = {\n\
    LONG: 101112\n\
  }\n\
  [6] = {\n\
    ULONG: 101112\n\
  }\n\
  [7] = {\n\
    DOUBLE: 131415.161700\n\
  }\n\
  [8] = {\n\
    STRING: >>>abc<<<\n\
  }\n\
  [9] = {\n\
    JSTRING: >>>[1,2,3]<<<\n\
  }\n\
  [10] = {\n\
    HASH: size=2 allocated=2\n\
    ['subhash-long'] = {\n\
      LONG: 1\n\
    }\n\
    ['subhash-int'] = {\n\
      INT: 2\n\
    }\n\
  }\n\
  [11] = {\n\
    ARRAY: size=2 allocated=2\n\
    [1] = {\n\
      INT: 4\n\
    }\n\
    [2] = {\n\
      LONG: 5\n\
    }\n\
  }\n");

  js = nro_to_json(ob);
  tlib_pass_if_true("populated array to json",
                    (0 != js)
                        && (0
                            == nr_strcmp(js,
                                         "\
[true,123,456,789,101112,101112,131415.16170,\"abc\",[1,2,3],{\"subhash-long\":1,\"subhash-int\":2},[4,5]]")),
                    "js=%s", js);

  tob = nro_create_from_json(js);
  nro_test("populated array from json", tob,
           "\
Object Dump (11):\n\
  ARRAY: size=11 allocated=16\n\
  [1] = {\n\
    BOOLEAN: 1\n\
  }\n\
  [2] = {\n\
    INT: 123\n\
  }\n\
  [3] = {\n\
    INT: 456\n\
  }\n\
  [4] = {\n\
    INT: 789\n\
  }\n\
  [5] = {\n\
    INT: 101112\n\
  }\n\
  [6] = {\n\
    INT: 101112\n\
  }\n\
  [7] = {\n\
    DOUBLE: 131415.161700\n\
  }\n\
  [8] = {\n\
    STRING: >>>abc<<<\n\
  }\n\
  [9] = {\n\
    ARRAY: size=3 allocated=8\n\
    [1] = {\n\
      INT: 1\n\
    }\n\
    [2] = {\n\
      INT: 2\n\
    }\n\
    [3] = {\n\
      INT: 3\n\
    }\n\
  }\n\
  [10] = {\n\
    HASH: size=2 allocated=8\n\
    ['subhash-long'] = {\n\
      INT: 1\n\
    }\n\
    ['subhash-int'] = {\n\
      INT: 2\n\
    }\n\
  }\n\
  [11] = {\n\
    ARRAY: size=2 allocated=8\n\
    [1] = {\n\
      INT: 4\n\
    }\n\
    [2] = {\n\
      INT: 5\n\
    }\n\
  }\n");

  nro_delete(tob);
  nr_free(js);

  /*
   * Verify using out of bounds index gives an error.
   */
  tob = nro_new_array();
  setcode = nro_set_array(tob, -1, ob);
  tlib_pass_if_true("out of bounds index", NR_FAILURE == setcode, "setcode=%d",
                    (int)setcode);

  /*
   * And that a valid one or 0 doesn't.
   */
  setcode = nro_set_array_int(tob, 1, 765);
  tlib_pass_if_true("set array", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);
  setcode = nro_set_array_int(tob, 0, 21);
  tlib_pass_if_true("set array", NR_SUCCESS == setcode, "setcode=%d",
                    (int)setcode);

  /*
   * Verify sizing is correct.
   */
  size = nro_getsize(ob);
  tlib_pass_if_true("correct size", 11 == size, "size=%d", size);

  nro_delete(tob);
  nro_delete(ob);
}

static void test_null_parameters(void) {
  nrobj_t* ob;
  nrotype_t t;
  int size;
  int i;
  char* js;

  t = nro_type(0);
  tlib_pass_if_true("null object has invalid type", NR_OBJECT_INVALID == t,
                    "t=%d", t);
  size = nro_getsize(0);
  tlib_pass_if_true("getsize fails on null object", -1 == size, "size=%d",
                    size);
  ob = nro_copy(0);
  tlib_pass_if_true("nro_copy fails on null object", 0 == ob, "ob=%p", ob);

  for (i = 1; i < num_otypes; i++) {
    ob = nro_assert(0, otypes[i]);
    tlib_pass_if_true("nro_assert on zero object", 0 == ob, "ob=%p", ob);
  }

  ob = nro_create_from_json(0);
  tlib_pass_if_true("object from null json", 0 == ob, "ob=%p", ob);

  js = nro_to_json(0);
  tlib_pass_if_true("json from null object", 0 == nr_strcmp(js, "null"),
                    "js=%s", js);
  nr_free(js);
}

static void test_nro_set_hash_failure(void) {
  nr_status_t setcode;
  nrobj_t* hash = nro_new_hash();
  nrobj_t* not_hash = nro_new_none();

  /*
   * Test : Null Object
   */
  setcode = nro_set_hash_none(0, "abc");
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_boolean(0, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_int(0, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_long(0, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_double(0, "abc", 1.1);
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_string(0, "abc", "string");
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_jstring(0, "abc", "\"jstring\"");
  tlib_pass_if_true("test_nro_set_hash_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : Null key
   */
  setcode = nro_set_hash_none(hash, 0);
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);
  setcode = nro_set_hash_boolean(hash, 0, 1);
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);
  setcode = nro_set_hash_int(hash, 0, 1);
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);
  setcode = nro_set_hash_long(hash, 0, 1);
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);
  setcode = nro_set_hash_double(hash, 0, 1.1);
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);
  setcode = nro_set_hash_string(hash, 0, "string");
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);
  setcode = nro_set_hash_jstring(hash, 0, "\"jstring\"");
  tlib_pass_if_true("test_nro_set_hash_failure null key", NR_FAILURE == setcode,
                    "setcode=%d", (int)setcode);

  /*
   * Test : Empty key
   */
  setcode = nro_set_hash_none(hash, "");
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_boolean(hash, "", 1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_int(hash, "", 1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_long(hash, "", 1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_double(hash, "", 1.1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_string(hash, "", "string");
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_jstring(hash, "", "\"jstring\"");
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : Not Hash
   */
  setcode = nro_set_hash_none(not_hash, "abc");
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_boolean(not_hash, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_int(not_hash, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_long(not_hash, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_double(not_hash, "abc", 1.1);
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_string(not_hash, "abc", "string");
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_hash_jstring(not_hash, "abc", "\"jstring\"");
  tlib_pass_if_true("test_nro_set_hash_failure empty key",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : Success, to validate previous tests
   */
  setcode = nro_set_hash_boolean(hash, "abc", 1);
  tlib_pass_if_true("test_nro_set_hash_failure success", NR_SUCCESS == setcode,
                    "setcode=%d", (int)setcode);

  nro_delete(hash);
  nro_delete(not_hash);
}

static void test_nro_set_array_failure(void) {
  nr_status_t setcode;
  nrobj_t* array = nro_new_array();
  nrobj_t* not_array = nro_new_none();

  /*
   * Test : Null Object
   */
  setcode = nro_set_array_none(0, 0);
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_boolean(0, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_int(0, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_long(0, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_double(0, 0, 1.1);
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_string(0, 0, "string");
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_jstring(0, 0, "\"jstring\"");
  tlib_pass_if_true("test_nro_set_array_failure null object",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : Negative Index
   */
  setcode = nro_set_array_none(array, -1);
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_boolean(array, -1, 1);
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_int(array, -1, 1);
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_long(array, -1, 1);
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_double(array, -1, 1.1);
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_string(array, -1, "string");
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_jstring(array, -1, "\"jstring\"");
  tlib_pass_if_true("test_nro_set_array_failure negative index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : High Index
   */
  setcode = nro_set_array_none(array, 1000000);
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_boolean(array, 1000000, 1);
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_int(array, 1000000, 1);
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_long(array, 1000000, 1);
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_double(array, 1000000, 1.1);
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_string(array, 1000000, "string");
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_jstring(array, 1000000, "\"jstring\"");
  tlib_pass_if_true("test_nro_set_array_failure high index",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : Not array
   */
  setcode = nro_set_array_none(not_array, 0);
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_boolean(not_array, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_int(not_array, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_long(not_array, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_double(not_array, 0, 1.1);
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_string(not_array, 0, "string");
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);
  setcode = nro_set_array_jstring(not_array, 0, "\"jstring\"");
  tlib_pass_if_true("test_nro_set_array_failure not array",
                    NR_FAILURE == setcode, "setcode=%d", (int)setcode);

  /*
   * Test : Success, to validate previous tests
   */
  setcode = nro_set_array_boolean(array, 0, 1);
  tlib_pass_if_true("test_nro_set_array_failure success", NR_SUCCESS == setcode,
                    "setcode=%d", (int)setcode);

  nro_delete(array);
  nro_delete(not_array);
}

static void test_create_from_json_unterminated(void) {
  nrobj_t* obj;
  char* json;

  obj = nro_create_from_json_unterminated(NULL, 0);
  tlib_pass_if_null("zero params", obj);

  obj = nro_create_from_json_unterminated("111", 0);
  tlib_pass_if_null("zero len", obj);

  obj = nro_create_from_json_unterminated("111", -1);
  tlib_pass_if_null("negative len", obj);

  obj = nro_create_from_json_unterminated(NULL, 2);
  tlib_pass_if_null("null json", obj);

  obj = nro_create_from_json_unterminated("111", 2);
  tlib_pass_if_not_null("success: len obeyed", obj);
  json = nro_to_json(obj);
  tlib_pass_if_str_equal("success: len obeyed", json, "11");
  nr_free(json);
  nro_delete(obj);
}

static void test_to_json_buffer(void) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  char* json;
  nrobj_t* obj = construct_hairy_object();

  tlib_fail_if_status_success("NULL buffer", nro_to_json_buffer(obj, NULL));

  tlib_pass_if_status_success("NULL object", nro_to_json_buffer(NULL, buf));
  tlib_pass_if_int_equal("NULL object writes null to buffer", 4,
                         nr_buffer_len(buf));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("NULL object writes null to buffer", "null",
                         nr_buffer_cptr(buf));
  nr_buffer_reset(buf);

  json = nro_to_json(obj);
  tlib_pass_if_status_success("hairy object", nro_to_json_buffer(obj, buf));
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("hairy object", json, nr_buffer_cptr(buf));

  nr_buffer_destroy(&buf);
  nr_free(json);
  nro_delete(obj);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* vp NRUNUSED) {
  test_basic_creation();
  test_create_objects();

  test_object_boolean();
  test_object_int();
  test_object_long();
  test_object_ulong();
  test_object_double();
  test_object_string();
  test_object_jstring();
  test_object_hash();
  test_object_array();
  test_null_parameters();

  test_find_array_int();

  test_incomensurate_get();

  test_nro_getival();
  test_nro_iteratehash();
  test_nro_hash_corner_cases();
  test_nro_array_corner_cases();
  test_nro_hairy_object_json();
  test_nro_hairy_utf8_object_json();
  test_nro_hairy_mangled_object_json();
  test_nro_json_corner_cases();
  test_nro_mangled_json();
  test_nro_set_hash_failure();
  test_nro_set_array_failure();

  test_create_from_json_unterminated();
  test_to_json_buffer();
}
