/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_environment.h"
#include "util_text.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

/*
 * Purpose : Tests if given a prefix a given key/value pair is added to a hash.
 * Params  : 1. prefix: The prefix to check the key against.
 *           2. key: The key to compare to the prefix.
 *           3. value: The value that corresponds to the key
 *           4. validCase: bool to indicate if the case should fail or succeed.
 * Returns : void
 */
static void test_nr_php_process_environment_variable_to_nrobj(
    const char* prefix,
    const char* key,
    const char* value,
    bool validCase) {
  nrobj_t* result_hash = NULL;
  nrobj_t* expect_hash = NULL;
  const char* r = NULL;
  nr_status_t err;
  char* result_str;
  char* expect_str;

  result_hash = nro_new_hash();
  nr_php_process_environment_variable_to_nrobj(prefix, key, value, result_hash);
  r = nro_get_hash_string(result_hash, key, &err);
  if (validCase) {
    tlib_pass_if_true("index OK", NR_SUCCESS == err, "success=%d", (int)err);
    tlib_pass_if_true("pick", 0 == nr_strcmp(r, NULL == value ? "" : value),
                      "r=%s but expected %s for key %s", r, value, key);
  } else {
    tlib_pass_if_false("index OK", NR_SUCCESS == err, "success=%d", (int)err);
    tlib_pass_if_null("NULL terms", r);
  }

  expect_hash = nro_new_hash();
  if (validCase) {
    nro_set_hash_string(expect_hash, key, value);
  }
  expect_str = nro_dump(expect_hash);
  result_str = nro_dump(result_hash);
  tlib_pass_if_true("contents", 0 == nr_strcmp(expect_str, result_str),
                    "\nresult_str=%s\nexpect_str=%s", result_str, expect_str);

  nr_free(expect_str);
  nr_free(result_str);
  nro_delete(expect_hash);
  nro_delete(result_hash);
}

/*
 * Purpose : Tests adding multiple key/value pairs to a hash.
 *
 * Returns : void
 */
static void test_multi_nr_php_process_environment_variable_to_nrobj() {
  nrobj_t* result_hash = NULL;
  nrobj_t* expect_hash = NULL;
  const char* r = NULL;
  nr_status_t err;
  char* result_str;
  char* expect_str;

  result_hash = nro_new_hash();
  /*
   * Add multiple key/value pairs to the hash including ones with duplicate
   * keys. The last added key should always take precedence over a previous
   * duplicate key.
   */
  nr_php_process_environment_variable_to_nrobj("MYPREFIX", "MYPREFIX_ONE",
                                               "one", result_hash);
  nr_php_process_environment_variable_to_nrobj("MYPREFIX", "MYPREFIX_TWO",
                                               "two", result_hash);
  nr_php_process_environment_variable_to_nrobj("MYPREFIX", "MYPREFIX_ONE",
                                               "second_one", result_hash);
  nr_php_process_environment_variable_to_nrobj("MYPREFIX", "MYPREFIX_ONE",
                                               "third_one", result_hash);
  nr_php_process_environment_variable_to_nrobj("MYPREFIX", "PREFIX_THREE",
                                               "three", result_hash);

  r = nro_get_hash_string(result_hash, "MYPREFIX_ONE", &err);

  tlib_pass_if_true("index OK", NR_SUCCESS == err, "success=%d", (int)err);
  tlib_pass_if_true("pick", 0 == nr_strcmp(r, "third_one"),
                    "r=%s but expected third_one", r);

  expect_hash = nro_new_hash();
  nro_set_hash_string(expect_hash, "MYPREFIX_ONE", "third_one");
  nro_set_hash_string(expect_hash, "MYPREFIX_TWO", "two");

  expect_str = nro_dump(expect_hash);
  result_str = nro_dump(result_hash);
  tlib_pass_if_true("contents", 0 == nr_strcmp(expect_str, result_str),
                    "\nresult_str=%s\nexpect_str=%s", result_str, expect_str);

  nr_free(expect_str);
  nr_free(result_str);
  nro_delete(expect_hash);
  nro_delete(result_hash);
}

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

/*
 * Purpose : Test the nr_php_process_environment_variables_to_nrobj
 * functionality.
 *
 * Returns : Void
 */
static void test_nr_php_process_environment_variables_to_nrobj(void) {
  /* Prefix and Key are same length, should fail because a value with only the
   * prefix is not valid.
   */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, "NR_METADATA_PREFIX_", "value", false);

  /* Valid prefix, key, value. Pair should be added to hash. */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, "NEW_RELIC_METADATA_ONE", "metadata_one", true);

  /* Non-matching prefix and key. Should not add pair to hash. */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, "OTHER", "metadata_two", false);

  /* Non-matching prefix and key. Should not add pair to hash. */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, "NEW_RELIC_THREE", "metadata_three", false);

  /* Null prefix should fail. Should not add pair to hash. */
  test_nr_php_process_environment_variable_to_nrobj(
      NULL, "NEW_RELIC_METADATA_FOUR", "metadata_four", false);

  /* Valid prefix, key, value. Pair should be added to hash. */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, "NEW_RELIC_METADATA_FIVE",
      "metadata_five with a space", true);

  /* Valid prefix, key, NULL value (acceptable). Pair should be added to hash.
   */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, "NEW_RELIC_METADATA_SIX", NULL, true);

  /* NULL key, NULL value. Pair should not be added to hash. */
  test_nr_php_process_environment_variable_to_nrobj(NR_METADATA_KEY_PREFIX,
                                                    NULL, NULL, false);

  /* NULL key. Pair should not be added to hash. */
  test_nr_php_process_environment_variable_to_nrobj(
      NR_METADATA_KEY_PREFIX, NULL, "metadata_seven", false);

  /* Should be able to add multiple valid pairs to hash. */
  test_multi_nr_php_process_environment_variable_to_nrobj();
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

/*
 * Purpose : Tests if given a prefix a given key/value pair is added to a hash.
 * Params  : 1. prefix: The prefix to check the key against.
 *           2. key: The key to compare to the prefix.
 *           3. value: The value that corresponds to the key
 *           4. expect_str: expected value.
 * Returns : void
 */
static void test_nr_php_process_environment_variable_to_string(
    const char* prefix,
    const char* key,
    const char* value,
    const char* expect_str) {
  char* result_str = NULL;

  result_str = nr_php_process_environment_variable_to_string(
      prefix, key, value, result_str, ":", ";");

  tlib_pass_if_true("contents", 0 == nr_strcmp(expect_str, result_str),
                    "\nresult_str=%s\nexpect_str=%s", result_str, expect_str);

  nr_free(result_str);
}

/*
 * Purpose : Tests adding multiple key/value pairs to a hash.
 *
 * Returns : void
 */
static void test_multi_nr_php_process_environment_variable_to_string() {
  char* result_str = NULL;
  char* expect_str = NULL;

  /*
   * Add multiple key/value pairs to the string including ones with duplicate
   * keys. The last added key will eventually take precedence over a previous
   * duplicate key when the string is eventually converted to a hash object.
   */
  result_str = nr_php_process_environment_variable_to_string(
      "MYPREFIX_", "MYPREFIX_ONE", "one", result_str, ":", ";");
  result_str = nr_php_process_environment_variable_to_string(
      "MYPREFIX_", "MYPREFIX_TWO", "two", result_str, ":", ";");
  result_str = nr_php_process_environment_variable_to_string(
      "MYPREFIX_", "MYPREFIX_ONE", "second_one", result_str, ":", ";");
  result_str = nr_php_process_environment_variable_to_string(
      "MYPREFIX_", "MYPREFIX_ONE", "third_one", result_str, ":", ";");
  result_str = nr_php_process_environment_variable_to_string(
      "MYPREFIX_", "PREFIX_THREE", "three", result_str, ":", ";");

  expect_str = nr_strdup("ONE:one;TWO:two;ONE:second_one;ONE:third_one");
  tlib_pass_if_true("contents", 0 == nr_strcmp(expect_str, result_str),
                    "\nresult_str=%s\nexpect_str=%s", result_str, expect_str);

  nr_free(expect_str);
  nr_free(result_str);
}

/*
 * Purpose : Test the nr_php_process_environment_variables_to_string
 * functionality.
 *
 * Returns : Void
 */
static void test_nr_php_process_environment_variables_to_string(void) {
  /* Prefix and Key are same length, should fail because a value with only the
   * prefix is not valid.
   */

  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, "NEW_RELIC_LABEL_", "value", NULL);

  /* Valid prefix, key, value. Pair should be added to string. */
  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, "NEW_RELIC_LABEL_ONE", "one", "ONE:one");

  /* Non-matching prefix and key. Should not add pair to string. */
  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, "OTHER", "two", NULL);

  /* Non-matching prefix and key. Should not add pair to string. */

  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, "NR_LABELS_THREE", "three", false);

  /* Null prefix should fail. Should not add pair to string. */
  test_nr_php_process_environment_variable_to_string(
      NULL, "NEW_RELIC_LABEL_FOUR", "four", NULL);

  /* Valid prefix, key, value. Pair should be added to string. */
  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, "NEW_RELIC_LABEL_FIVE",
      "metadata_five with a space", "FIVE:metadata_five with a space");

  /* Valid prefix, key, NULL value (acceptable). Pair should be added to string.
   */
  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, "NEW_RELIC_LABEL_SIX", NULL, "SIX");

  /* NULL key, NULL value. Pair should not be added to string. */
  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, NULL, NULL, NULL);

  /* NULL key. Pair should not be added to string. */
  test_nr_php_process_environment_variable_to_string(
      NR_LABELS_SINGULAR_KEY_PREFIX, NULL, "seven", NULL);

  /* Should be able to add multiple valid pairs to string. */
  test_multi_nr_php_process_environment_variable_to_string();
}

static void test_cross_agent_docker_v2(void) {
  int i;
  char* json;
  nrobj_t* tests;

#define DOCKER_V2_TESTS_PATH CROSS_AGENT_TESTS_DIR "/docker_container_id_v2/"

  json = nr_read_file_contents(DOCKER_V2_TESTS_PATH "cases.json",
                               10 * 1000 * 1000);
  tlib_pass_if_not_null(DOCKER_V2_TESTS_PATH "cases.json readable", json);
  tests = nro_create_from_json(json);
  nr_free(json);

  for (i = 1; i <= nro_getsize(tests); i++) {
    const nrobj_t* test = NULL;
    const char* filename = NULL;
    const char* expectedID = NULL;
    const nrobj_t* expectedMetrics = NULL;
    char* full_filename = NULL;
    char* detectedID = NULL;

    test = nro_get_array_hash(tests, i, NULL);
    tlib_pass_if_true("test valid", NULL != test, "test=%p", test);
    filename = nro_get_hash_string(test, "filename", NULL);
    expectedID = nro_get_hash_string(test, "containerId", NULL);
    expectedMetrics = nro_get_hash_array(test, "expectedMetrics", NULL);
    /* not currently inspected so this avoids a compiler error */
    (void)expectedMetrics;

    tlib_pass_if_true("filname valid", NULL != filename, "filename=%p",
                      filename);

    full_filename
        = nr_str_append(nr_strdup(DOCKER_V2_TESTS_PATH), filename, "");
    detectedID = nr_php_parse_v2_docker_id(full_filename);
    nr_free(full_filename);
    tlib_pass_if_str_equal("Match Docker cgroup v2 ID", expectedID, detectedID);
    nr_free(detectedID);
  }

  nro_delete(tests);
}

static void test_docker_v2(void) {
  char* detectedID = NULL;

  // handles bad values without problems
  detectedID = nr_php_parse_v2_docker_id(NULL);
  tlib_pass_if_null("NULL filename returns NULL", detectedID);

  detectedID = nr_php_parse_v2_docker_id("");
  tlib_pass_if_null("Empty filename returns NULL", detectedID);

  detectedID = nr_php_parse_v2_docker_id("/dev/null");
  tlib_pass_if_null("/dev/null returns NULL", detectedID);
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");

  test_rocket_assignments();

  test_nr_php_process_environment_variables_to_nrobj();

  test_nr_php_process_environment_variables_to_string();

  tlib_php_engine_destroy();

  test_cross_agent_docker_v2();

  test_docker_v2();
}
