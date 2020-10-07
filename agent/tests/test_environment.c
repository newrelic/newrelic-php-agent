/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_environment.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_single_rocket_assignment(const char* key, const char* value) {
  nrobj_t* result_env = NULL;
  nrobj_t* expect_env = NULL;
  char* s = NULL;
  const char* r = NULL;
  nr_status_t err;
  char buf[BUFSIZ];
  char* result_str;
  char* expect_str;

  snprintf(buf, sizeof(buf), "\n%s => %s\n", key, value);
  result_env = nro_new_hash();
  s = nr_strdup(buf);
  nr_php_parse_rocket_assignment_list(s, nr_strlen(s), result_env);
  r = nro_get_hash_string(result_env, key, &err);
  tlib_pass_if_true("index OK", NR_SUCCESS == err, "success=%d", (int)err);
  tlib_pass_if_true("pick", 0 == nr_strcmp(r, value), "r=%s but expected %s", r,
                    value);
  nr_free(s);

  expect_env = nro_new_hash();
  nro_set_hash_string(expect_env, key, value);
  expect_str = nro_dump(expect_env);
  result_str = nro_dump(result_env);
  tlib_pass_if_true("contents", 0 == nr_strcmp(expect_str, result_str),
                    "\nresult_str=%s\nexpect_str=%s", result_str, expect_str);

  nr_free(expect_str);
  nr_free(result_str);
  nro_delete(expect_env);
  nro_delete(result_env);
}

#define test_rocket_assignment_string_to_obj(S, E) \
  test_rocket_assignment_string_to_obj_fn((S), (E), __FILE__, __LINE__)
static void test_rocket_assignment_string_to_obj_fn(const char* stimulus,
                                                    nrobj_t* expect_env,
                                                    const char* file,
                                                    int line) {
  char* s = NULL;
  nrobj_t* result_env = NULL;
  char* result;
  char* expect;

  result_env = nro_new_hash();
  s = nr_strdup(stimulus);
  nr_php_parse_rocket_assignment_list(s, nr_strlen(s), result_env);
  result = nro_dump(result_env);
  expect = nro_dump(expect_env);
  test_pass_if_true("object identical", 0 == nr_strcmp(expect, result),
                    "\nexpect=%d: %s\nresult=%d: %s", nr_strlen(expect), expect,
                    nr_strlen(result), result);
  nr_free(result);
  nr_free(expect);
  nr_free(s);
  nro_delete(result_env);
}

static void test_rocket_assignments(void) {
  nrobj_t* expect_env = NULL;

  test_single_rocket_assignment("x", "17");
  test_single_rocket_assignment("xxxx", "17");
  test_single_rocket_assignment("x xx", "17");
  test_single_rocket_assignment(" x", "17");
  test_single_rocket_assignment("x ", "17");
  test_single_rocket_assignment("x", " 17");
  test_single_rocket_assignment("x", "17 ");

  test_single_rocket_assignment("=>", "17");
  test_single_rocket_assignment("XXXX", "=>");
  test_single_rocket_assignment("X XXX", "=>");

  expect_env = nro_new_hash();
  test_rocket_assignment_string_to_obj(0, expect_env);
  test_rocket_assignment_string_to_obj("\n", expect_env);
  test_rocket_assignment_string_to_obj("", expect_env);

  test_rocket_assignment_string_to_obj("\n\n\n", expect_env);

  nro_set_hash_string(expect_env, "foo", "17");
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo => 17"
      "\n",
      expect_env);
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo => 17"
      "\n"
      "\n",
      expect_env);
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo => 17"
      "\n"
      "bar =>",
      expect_env);
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo => 18"
      "\n"
      "foo => 17\n",
      expect_env);

  /*
   * This tests some unintentional non-spec-conforming behavior.
   * The char immediately after newline gets dropped,
   * but the assignment still gets processed.
   */
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo =\n117"
      "\n",
      expect_env);

  /*
   * Test multiple assignments.
   */
  nro_set_hash_string(expect_env, "bar", "18");
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo => 17"
      "\n"
      "bar => 18\n",
      expect_env);
  test_rocket_assignment_string_to_obj(
      "\n"
      "foo => 17"
      "\n\n\n"
      "bar => 18\n",
      expect_env);

  /*
   * Test spaces in key/value strings both before and after the "=>".
   */
  nro_delete(expect_env);
  expect_env = nro_new_hash();
  nro_set_hash_string(expect_env, "f o o", "1 7");
  nro_set_hash_string(expect_env, "b ar", "18 19");
  test_rocket_assignment_string_to_obj(
      "\n"
      "f o o => 1 7"
      "\n  \n\n"
      "b ar => 18 19\n",
      expect_env);

  nro_delete(expect_env);
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_rocket_assignments();

  tlib_php_engine_destroy(TSRMLS_C);
}
