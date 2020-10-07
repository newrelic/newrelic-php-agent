/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_rules.h"
#include "util_metrics.h"
#include "util_object.h"
#include "util_reply.h"

#include "tlib_main.h"

nr_rules_result_t nr_rules_apply(const nrrules_t* rules NRUNUSED,
                                 const char* name NRUNUSED,
                                 char** new_name NRUNUSED) {
  return NR_RULES_RESULT_UNCHANGED;
}

static void test_reply_get_int(const nrobj_t* reply,
                               const nrobj_t* arr,
                               const nrobj_t* empty) {
  int rv;

  rv = nr_reply_get_int(0, 0, 11);
  tlib_pass_if_true("null params", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_int(0, "intkey", 11);
  tlib_pass_if_true("null reply", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_int(reply, 0, 11);
  tlib_pass_if_true("null name", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_int(reply, "", 11);
  tlib_pass_if_true("empty name", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_int(arr, "intkey", 11);
  tlib_pass_if_true("reply is wrong type", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_int(empty, "intkey", 11);
  tlib_pass_if_true("reply is empty", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_int(reply, "hashkey", 11);
  tlib_pass_if_true("name is wrong type", 11 == rv, "rv=%d", rv);

  rv = nr_reply_get_int(reply, "intkey", 11);
  tlib_pass_if_true("success int", 22 == rv, "rv=%d", rv);
}

static void test_reply_get_bool(const nrobj_t* reply,
                                const nrobj_t* arr,
                                const nrobj_t* empty) {
  int rv;

  rv = nr_reply_get_bool(0, 0, 11);
  tlib_pass_if_true("null params", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(0, "intkey", 11);
  tlib_pass_if_true("null reply", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, 0, 11);
  tlib_pass_if_true("null name", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "", 11);
  tlib_pass_if_true("empty name", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(arr, "intkey", 11);
  tlib_pass_if_true("reply is wrong type", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(empty, "intkey", 11);
  tlib_pass_if_true("reply is empty", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "hashkey", 11);
  tlib_pass_if_true("name is wrong type", 11 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "stringkey", 11);
  tlib_pass_if_true("unknown string", 11 == rv, "rv=%d", rv);

  rv = nr_reply_get_bool(reply, "intkey", 11);
  tlib_pass_if_true("success int", 22 == rv, "rv=%d", rv);

  rv = nr_reply_get_bool(reply, "1key", 11);
  tlib_pass_if_true("success 1key", 1 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "ykey", 11);
  tlib_pass_if_true("success ykey", 1 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "Ykey", 11);
  tlib_pass_if_true("success Ykey", 1 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "tkey", 11);
  tlib_pass_if_true("success tkey", 1 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "Tkey", 11);
  tlib_pass_if_true("success Tkey", 1 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "onkey", 11);
  tlib_pass_if_true("success onkey", 1 == rv, "rv=%d", rv);

  rv = nr_reply_get_bool(reply, "0key", 11);
  tlib_pass_if_true("success 0key", 0 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "nkey", 11);
  tlib_pass_if_true("success nkey", 0 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "Nkey", 11);
  tlib_pass_if_true("success Nkey", 0 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "fkey", 11);
  tlib_pass_if_true("success fkey", 0 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "Fkey", 11);
  tlib_pass_if_true("success Fkey", 0 == rv, "rv=%d", rv);
  rv = nr_reply_get_bool(reply, "offkey", 11);
  tlib_pass_if_true("success offkey", 0 == rv, "rv=%d", rv);
}

static void test_reply_get_double(const nrobj_t* reply,
                                  const nrobj_t* arr,
                                  const nrobj_t* empty) {
  double rv;

  rv = nr_reply_get_double(0, 0, 1.1);
  tlib_pass_if_true("null params", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(0, "intkey", 1.1);
  tlib_pass_if_true("null reply", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(reply, 0, 1.1);
  tlib_pass_if_true("null name", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(reply, "", 1.1);
  tlib_pass_if_true("empty name", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(arr, "intkey", 1.1);
  tlib_pass_if_true("reply is wrong type", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(empty, "intkey", 1.1);
  tlib_pass_if_true("reply is empty", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(reply, "hashkey", 1.1);
  tlib_pass_if_true("name is wrong type", 1.1 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(reply, "stringkey", 1.1);
  tlib_pass_if_true("name is wrong type", 1.1 == rv, "rv=%f", rv);

  rv = nr_reply_get_double(reply, "intkey", 1.1);
  tlib_pass_if_true("success int", 22 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(reply, "longkey", 1.1);
  tlib_pass_if_true("success long", 44 == rv, "rv=%f", rv);
  rv = nr_reply_get_double(reply, "doublekey", 1.1);
  tlib_pass_if_true("success double", 12.5 == rv, "rv=%f", rv);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  nrobj_t* reply;
  nrobj_t* arr;
  nrobj_t* empty;

  reply = nro_new_hash();
  arr = nro_new_array();
  empty = nro_new_hash();

  nro_set_hash(reply, "hashkey", empty);
  nro_set_hash_int(reply, "intkey", 22);
  nro_set_hash_long(reply, "longkey", 44);
  nro_set_hash_double(reply, "doublekey", 12.5);
  nro_set_hash_string(reply, "stringkey", "stringvalue");

  nro_set_hash_string(reply, "1key", "1value");
  nro_set_hash_string(reply, "ykey", "yvalue");
  nro_set_hash_string(reply, "Ykey", "Yvalue");
  nro_set_hash_string(reply, "tkey", "tvalue");
  nro_set_hash_string(reply, "Tkey", "Tvalue");
  nro_set_hash_string(reply, "onkey", "on");

  nro_set_hash_string(reply, "0key", "0value");
  nro_set_hash_string(reply, "nkey", "nvalue");
  nro_set_hash_string(reply, "Nkey", "Nvalue");
  nro_set_hash_string(reply, "fkey", "fvalue");
  nro_set_hash_string(reply, "Fkey", "Fvalue");
  nro_set_hash_string(reply, "offkey", "off");

  test_reply_get_int(reply, arr, empty);
  test_reply_get_bool(reply, arr, empty);
  test_reply_get_double(reply, arr, empty);

  nro_delete(empty);
  nro_delete(reply);
  nro_delete(arr);
}
