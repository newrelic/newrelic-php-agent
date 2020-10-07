/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_labels.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

#define labels_cross_agent_testcase(...) \
  labels_cross_agent_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void labels_cross_agent_testcase_fn(const char* testname,
                                           const char* input,
                                           const char* expected_json,
                                           const char* file,
                                           int line) {
  nrobj_t* obj;
  nrobj_t* formatted_obj;
  char* actual_json;

  obj = nr_labels_parse(input);
  formatted_obj = nr_labels_connector_format(obj);
  actual_json = nro_to_json(formatted_obj);

  test_pass_if_true(testname, 0 == nr_strcmp(actual_json, expected_json),
                    "actual_json=%s expected_json=%s", NRSAFESTR(actual_json),
                    NRSAFESTR(expected_json));

  nr_free(actual_json);
  nro_delete(formatted_obj);
  nro_delete(obj);
}

#define labels_testcase(...) labels_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void labels_testcase_fn(const char* input,
                               const char* expected_json,
                               const char* file,
                               int line) {
  nrobj_t* obj;
  char* actual_json;

  obj = nr_labels_parse(input);
  actual_json = nro_to_json(obj);

  test_pass_if_true(input, 0 == nr_strcmp(actual_json, expected_json),
                    "actual_json=%s expected_json=%s", NRSAFESTR(actual_json),
                    NRSAFESTR(expected_json));

  nr_free(actual_json);
  nro_delete(obj);
}

static void test_nr_labels_parse(void) {
  labels_testcase("alpha:beta", "{\"alpha\":\"beta\"}");
  labels_testcase("alpha:beta;", "{\"alpha\":\"beta\"}");
  labels_testcase("alpha:beta;foo:bar", "{\"alpha\":\"beta\",\"foo\":\"bar\"}");
  labels_testcase("alpha:beta;foo:bar;",
                  "{\"alpha\":\"beta\",\"foo\":\"bar\"}");
  labels_testcase("alpha:beta;;;;foo:bar;;;;", "null");
  labels_testcase("alpha:beta;alpha:gamma", "{\"alpha\":\"gamma\"}");

  labels_testcase(NULL, "null");
  labels_testcase("", "null");
  labels_testcase(";", "null");

  labels_testcase(":", "null");
  labels_testcase(":;", "null");
  labels_testcase(";:", "null");
  labels_testcase("::", "null");

  labels_testcase(";;;;", "null");
  labels_testcase("    ", "null");
  labels_testcase(" ; : ; ", "null");
  labels_testcase(" ;  :a; ", "null");
  labels_testcase(" ;a :  ; ", "null");
}

static void test_nr_labels_parse_and_format_cross_agent(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define LABELS_CLEAN_TESTS_FILE CROSS_AGENT_TESTS_DIR "/labels.json"
  json = nr_read_file_contents(LABELS_CLEAN_TESTS_FILE, 10 * 1000 * 1000);
  tlib_pass_if_true("tests valid", 0 != json, "json=%p", json);

  if (0 == json) {
    return;
  }

  array = nro_create_from_json(json);
  tlib_pass_if_true("tests valid", 0 != array, "array=%p", array);
  otype = nro_type(array);
  tlib_pass_if_true("tests valid", NR_OBJECT_ARRAY == otype, "otype=%d",
                    (int)otype);

  if (array && (NR_OBJECT_ARRAY == nro_type(array))) {
    for (i = 1; i <= nro_getsize(array); i++) {
      const nrobj_t* hash = nro_get_array_hash(array, i, 0);
      const char* name = nro_get_hash_string(hash, "name", 0);
      const char* labelString = nro_get_hash_string(hash, "labelString", 0);
      const nrobj_t* expected = nro_get_hash_array(hash, "expected", 0);

      tlib_pass_if_true("tests valid", 0 != name, "name=%p", name);
      tlib_pass_if_true("tests valid", 0 != labelString, "labelString=%p",
                        labelString);
      tlib_pass_if_true("tests valid", 0 != expected, "expected=%p", expected);

      if ((0 == nr_strcmp(name, "long_4byte_utf8"))
          || (0 == nr_strcmp(name, "long_multibyte_utf8"))) {
        continue;
      }

      if (name && labelString && expected) {
        if (expected && (NR_OBJECT_ARRAY == nro_type(expected))) {
          char* expected_json = nro_to_json(expected);
          labels_cross_agent_testcase(name, labelString, expected_json);
          nr_free(expected_json);
        }
      }
    }
  }

  nro_delete(array);
  nr_free(json);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_nr_labels_parse();
  test_nr_labels_parse_and_format_cross_agent();
}
