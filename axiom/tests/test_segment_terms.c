/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_segment_terms.h"
#include "nr_segment_terms_private.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

static void test_segment_terms_create_destroy(void) {
  nr_segment_terms_t* terms;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("0 size", nr_segment_terms_create(0));
  tlib_pass_if_null("negative size", nr_segment_terms_create(-1));

  /*
   * Test : Creation.
   */
  terms = nr_segment_terms_create(10);
  tlib_pass_if_not_null("created", terms);
  tlib_pass_if_int_equal("capacity", 10, terms->capacity);
  tlib_pass_if_int_equal("size", 0, terms->size);
  tlib_pass_if_not_null("rules", terms->rules);

  /*
   * Test : Destruction.
   */
  nr_segment_terms_destroy(&terms);
  tlib_pass_if_null("destroyed", terms);
}

static void test_segment_terms_create_from_obj(void) {
  nrobj_t* obj;
  nr_segment_terms_t* terms;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL obj", nr_segment_terms_create_from_obj(NULL));

  obj = nro_new_hash();
  tlib_pass_if_null("non-array obj", nr_segment_terms_create_from_obj(obj));
  nro_delete(obj);

  obj = nro_new_array();
  tlib_pass_if_null("empty obj", nr_segment_terms_create_from_obj(obj));
  nro_delete(obj);

  obj = nro_create_from_json("[[]]");
  tlib_pass_if_null("malformed obj", nr_segment_terms_create_from_obj(obj));
  nro_delete(obj);

  obj = nro_create_from_json("[{}]");
  tlib_pass_if_null("malformed obj", nr_segment_terms_create_from_obj(obj));
  nro_delete(obj);

  /*
   * Test : Normal operation.
   */
  obj = nro_create_from_json(
      "["
      "{\"prefix\":\"Foo/Bar\",\"terms\":[\"a\",\"b\"]},"
      "{\"prefix\":\"Bar/Foo\",\"terms\":[\"c\",\"d\"]}"
      "]");

  terms = nr_segment_terms_create_from_obj(obj);
  tlib_pass_if_not_null("well formed obj", terms);
  tlib_pass_if_int_equal("terms capacity", 2, terms->capacity);
  tlib_pass_if_int_equal("terms size", 2, terms->size);
  tlib_pass_if_str_equal("rule prefix", "Foo/Bar/", terms->rules[0]->prefix);

  nro_delete(obj);
  nr_segment_terms_destroy(&terms);
}

static void test_segment_terms_add(void) {
  const char* prefix = "Foo/Bar";
  nr_segment_terms_t* terms;
  nrobj_t* whitelist;

  terms = nr_segment_terms_create(2);
  whitelist = nro_create_from_json("[\"a\",\"b\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL segment terms",
                              nr_segment_terms_add(NULL, prefix, whitelist));
  tlib_pass_if_status_failure("NULL prefix",
                              nr_segment_terms_add(terms, NULL, whitelist));
  tlib_pass_if_status_failure("empty prefix",
                              nr_segment_terms_add(terms, "", whitelist));
  tlib_pass_if_status_failure("NULL whitelist",
                              nr_segment_terms_add(terms, prefix, NULL));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_status_success("add term",
                              nr_segment_terms_add(terms, prefix, whitelist));
  tlib_pass_if_int_equal("terms size", 1, terms->size);

  tlib_pass_if_status_success("add term",
                              nr_segment_terms_add(terms, prefix, whitelist));
  tlib_pass_if_int_equal("terms size", 2, terms->size);

  /*
   * Test : Full terms.
   */
  tlib_pass_if_status_failure("add term",
                              nr_segment_terms_add(terms, prefix, whitelist));
  tlib_pass_if_int_equal("terms size", 2, terms->size);

  nro_delete(whitelist);
  nr_segment_terms_destroy(&terms);
}

static void test_segment_terms_add_from_obj(void) {
  nrobj_t* invalid_rule;
  nrobj_t* rule;
  nr_segment_terms_t* terms;

  rule = nro_create_from_json(
      "{\"prefix\":\"Foo/Bar\",\"terms\":[\"a\",\"b\"]}");
  terms = nr_segment_terms_create(2);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL segment terms",
                              nr_segment_terms_add_from_obj(NULL, rule));
  tlib_pass_if_status_failure("NULL rule",
                              nr_segment_terms_add_from_obj(terms, NULL));

  invalid_rule = nro_new_array();
  tlib_pass_if_status_failure(
      "non-object rule", nr_segment_terms_add_from_obj(terms, invalid_rule));
  nro_delete(invalid_rule);

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_status_success("add term",
                              nr_segment_terms_add_from_obj(terms, rule));
  tlib_pass_if_int_equal("terms size", 1, terms->size);
  tlib_pass_if_str_equal("rule prefix", "Foo/Bar/", terms->rules[0]->prefix);

  tlib_pass_if_status_success("add term",
                              nr_segment_terms_add_from_obj(terms, rule));
  tlib_pass_if_int_equal("terms size", 2, terms->size);

  /*
   * Test : Full terms.
   */
  tlib_pass_if_status_failure("add term",
                              nr_segment_terms_add_from_obj(terms, rule));
  tlib_pass_if_int_equal("terms size", 2, terms->size);

  nro_delete(rule);
  nr_segment_terms_destroy(&terms);
}

static void test_segment_terms_apply(void) {
  int i;
  char* json;
  nrobj_t* tests;

#define SEGMENT_TERMS_TESTS_FILE \
  CROSS_AGENT_TESTS_DIR "/transaction_segment_terms.json"
  json = nr_read_file_contents(SEGMENT_TERMS_TESTS_FILE, 10 * 1000 * 1000);
  tlib_pass_if_not_null(SEGMENT_TERMS_TESTS_FILE " readable", json);
  tests = nro_create_from_json(json);
  nr_free(json);

  for (i = 1; i <= nro_getsize(tests); i++) {
    nr_segment_terms_t* terms;
    const nrobj_t* test;
    const nrobj_t* rules;
    const nrobj_t* testcases;
    const char* name;
    int j;

    test = nro_get_array_hash(tests, i, NULL);
    name = nro_get_hash_string(test, "testname", NULL);
    rules = nro_get_hash_array(test, "transaction_segment_terms", NULL);
    testcases = nro_get_hash_array(test, "tests", NULL);
    terms = nr_segment_terms_create(nro_getsize(rules));

    for (j = 1; j <= nro_getsize(rules); j++) {
      nr_segment_terms_add_from_obj(terms, nro_get_array_hash(rules, j, NULL));
    }

    for (j = 1; j <= nro_getsize(testcases); j++) {
      const char* expected;
      const char* input;
      const nrobj_t* testcase;
      char* result;

      testcase = nro_get_array_hash(testcases, j, NULL);
      expected = nro_get_hash_string(testcase, "expected", NULL);
      input = nro_get_hash_string(testcase, "input", NULL);

      result = nr_segment_terms_apply(terms, input);
      tlib_pass_if_str_equal(name, expected, result);

      nr_free(result);
    }

    nr_segment_terms_destroy(&terms);
  }

  nro_delete(tests);
}

static void test_segment_terms_rule_create_destroy(void) {
  nr_segment_terms_rule_t* rule;
  nrobj_t* invalid_terms;
  nrobj_t* terms;

  invalid_terms = nro_new_long(2);
  terms = nro_create_from_json("[\"a\",\"b\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL prefix", nr_segment_terms_rule_create(NULL, terms));
  tlib_pass_if_null("empty prefix", nr_segment_terms_rule_create("", terms));
  tlib_pass_if_null("NULL terms",
                    nr_segment_terms_rule_create("Foo/Bar", NULL));
  tlib_pass_if_null("invalid terms",
                    nr_segment_terms_rule_create("Foo/Bar", invalid_terms));

  /*
   * Test : Creation.
   */
  rule = nr_segment_terms_rule_create("Foo/Bar", terms);
  tlib_pass_if_not_null("creation", rule);
  tlib_pass_if_str_equal("prefix", "Foo/Bar/", rule->prefix);
  tlib_pass_if_int_equal("prefix length", 8, rule->prefix_len);
  tlib_pass_if_not_null("regex", rule->re);

  nr_segment_terms_rule_destroy(&rule);
  rule = nr_segment_terms_rule_create("Foo/Bar/", terms);
  tlib_pass_if_not_null("creation", rule);
  tlib_pass_if_str_equal("prefix", "Foo/Bar/", rule->prefix);
  tlib_pass_if_int_equal("prefix length", 8, rule->prefix_len);
  tlib_pass_if_not_null("regex", rule->re);

  /*
   * Test : Destruction.
   */
  nr_segment_terms_rule_destroy(&rule);
  tlib_pass_if_null("destruction", rule);

  nro_delete(invalid_terms);
  nro_delete(terms);
}

static void test_segment_terms_rule_build_regex(void) {
  char* regex = NULL;
  nrobj_t* invalid_terms;
  nrobj_t* terms;

  invalid_terms = nro_new_hash();
  terms = nro_new_array();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL terms", nr_segment_terms_rule_build_regex(NULL));
  tlib_pass_if_null("invalid terms",
                    nr_segment_terms_rule_build_regex(invalid_terms));

  /*
   * Test : Empty terms.
   */
  regex = nr_segment_terms_rule_build_regex(terms);
  tlib_pass_if_str_equal("empty terms", "$.", regex);
  nr_free(regex);

  /*
   * Test : One term.
   */
  nro_delete(terms);
  terms = nro_create_from_json("[\"a\"]");
  regex = nr_segment_terms_rule_build_regex(terms);
  tlib_pass_if_str_equal("one term", "(a)", regex);
  nr_free(regex);

  /*
   * Test : Two terms.
   */
  nro_delete(terms);
  terms = nro_create_from_json("[\"a\",\"b:c\"]");
  regex = nr_segment_terms_rule_build_regex(terms);
  tlib_pass_if_str_equal("two terms", "(a)|(b\\:c)", regex);
  nr_free(regex);

  nro_delete(invalid_terms);
  nro_delete(terms);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_segment_terms_create_destroy();
  test_segment_terms_create_from_obj();
  test_segment_terms_add();
  test_segment_terms_add_from_obj();
  test_segment_terms_apply();
  test_segment_terms_rule_create_destroy();
  test_segment_terms_rule_build_regex();
}
