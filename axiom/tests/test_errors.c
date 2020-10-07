/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_errors.h"
#include "nr_errors_private.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

#define test_string_is_valid_json(...) \
  test_string_is_valid_json_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_string_is_valid_json_fn(const char* testname,
                                         const char* str,
                                         const char* file,
                                         int line) {
  nrobj_t* obj;

  obj = nro_create_from_json(str);
  test_pass_if_true(testname, 0 != obj, "str=%s", NRSAFESTR(str));
  nro_delete(obj);
}

static void test_error_create_bad_params(void) {
  nr_error_t* error;
  int priority = 5;
  nrtime_t when = 1378167 * NR_TIME_DIVISOR_MS;

  error = nr_error_create(0, NULL, NULL, NULL, NULL, when);
  tlib_pass_if_true("zero params", NULL == error, "error=%p", error);
  nr_error_destroy(&error);

  error = nr_error_create(priority, NULL, "my/class", "[\"already\\/escaped\"]",
                          "my/span_id", when);
  tlib_pass_if_true("null message", NULL == error, "error=%p", error);
  nr_error_destroy(&error);

  error = nr_error_create(priority, "my/message", NULL,
                          "[\"already\\/escaped\"]", "my/span_id", when);
  tlib_pass_if_true("null class", NULL == error, "error=%p", error);
  nr_error_destroy(&error);

  error = nr_error_create(priority, "my/message", "my/class", NULL,
                          "my/span_id", when);
  tlib_pass_if_true("null stacktrace_json", NULL == error, "error=%p", error);
  nr_error_destroy(&error);

  error = nr_error_create(priority, "my/message", "my/class",
                          "[\"already\\/escaped\"]", NULL, when);
  tlib_pass_if_true("null span_id", 0 != error, "error=%p", error);
  nr_error_destroy(&error);
}

static void test_error_create_priority_and_destroy(void) {
  nr_error_t* error;
  int priority = 5;
  nrtime_t when = 1378167 * NR_TIME_DIVISOR_MS;
  int actual_priority;
  char* json;

  error = nr_error_create(priority, "my/message", "my/class",
                          "[\"already\\/escaped\"]", "my/span_id", when);
  tlib_pass_if_true("error created", 0 != error, "error=%p", error);

  actual_priority = nr_error_priority(error);
  tlib_pass_if_true(
      "error created has correct priority", priority == actual_priority,
      "priority=%d actual_priority=%d", priority, actual_priority);

  json = nr_error_to_daemon_json(error, "my/txn", 0, 0, 0, 0);
  tlib_pass_if_str_equal(
      "error created", json,
      "[1378167,\"my\\/txn\",\"my\\/message\",\"my\\/class\","
      "{\"stack_trace\":[\"already\\/escaped\"]}]");
  test_string_is_valid_json("error created", json);
  nr_free(json);

  nr_error_destroy(&error);
}

static void test_error_priority_bad_params(void) {
  int priority = nr_error_priority(0);

  tlib_pass_if_true("priority of null error is zero", 0 == priority,
                    "priority=%d", priority);
}

static void test_error_destroy_bad_params(void) {
  nr_error_t* error = 0;

  /* Don't blow up! */
  nr_error_destroy(0);
  nr_error_destroy(&error);
}

static void test_getters(void) {
  nr_error_t* error;
  int priority = 5;
  nrtime_t when = 1378167 * NR_TIME_DIVISOR_MS;

  tlib_pass_if_null("error msg null error", nr_error_get_message(NULL));
  tlib_pass_if_null("error klass null error", nr_error_get_klass(NULL));
  tlib_pass_if_null("error span_id null error", nr_error_get_span_id(NULL));
  tlib_pass_if_uint64_t_equal("error time null error", 0,
                              nr_error_get_time(NULL));

  error = nr_error_create(priority, "my/message", "my/class", "[]",
                          "my/span_id", when);

  tlib_pass_if_str_equal("error message getter success", "my/message",
                         nr_error_get_message(error));
  tlib_pass_if_str_equal("error klass getter success", "my/class",
                         nr_error_get_klass(error));
  tlib_pass_if_str_equal("error klass getter success", "my/span_id",
                         nr_error_get_span_id(error));
  tlib_pass_if_uint64_t_equal("null error", when, nr_error_get_time(error));

  nr_error_destroy(&error);
}

static void test_error_to_daemon_json(void) {
  nr_error_t* error;
  const char* txn_name = "my_txn_name";
  nrobj_t* agent_attributes = nro_create_from_json("{\"agent_attributes\":1}");
  nrobj_t* user_attributes = nro_create_from_json("{\"user_attributes\":1}");
  nrobj_t* intrinsics = nro_create_from_json("{\"intrinsics\":1}");
  const char* request_uri = "my_request_uri";
  char* json;
  nrtime_t when = 123 * NR_TIME_DIVISOR;
  const char* msg = "my_msg";
  const char* klass = "my_klass";
  const char* span_id = "my_span_id";
  const char* stacktrace_json = "[]";
  int priority = 5;

  error = nr_error_create(priority, msg, klass, stacktrace_json, span_id, when);

  json = nr_error_to_daemon_json(error, txn_name, agent_attributes,
                                 user_attributes, intrinsics, request_uri);
  tlib_pass_if_str_equal("success daemon json", json,
                         "[123000,\"my_txn_name\",\"my_msg\",\"my_klass\","
                         "{"
                         "\"stack_trace\":[],"
                         "\"agentAttributes\":{\"agent_attributes\":1},"
                         "\"userAttributes\":{\"user_attributes\":1},"
                         "\"intrinsics\":{\"intrinsics\":1},"
                         "\"request_uri\":\"my_request_uri\""
                         "}"
                         "]");
  test_string_is_valid_json("success daemon json", json);
  nr_free(json);

  json = nr_error_to_daemon_json(NULL, txn_name, agent_attributes,
                                 user_attributes, intrinsics, request_uri);
  tlib_pass_if_null("NULL error", json);
  nr_free(json);

  json = nr_error_to_daemon_json(error, NULL, 0, 0, 0, 0);
  tlib_pass_if_str_equal("no txn fields", json,
                         "[123000,\"\",\"my_msg\",\"my_klass\",{"
                         "\"stack_trace\":[]}]");
  test_string_is_valid_json("no txn fields", json);
  nr_free(json);

  nr_error_destroy(&error);
  nro_delete(agent_attributes);
  nro_delete(user_attributes);
  nro_delete(intrinsics);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_error_create_bad_params();
  test_error_create_priority_and_destroy();
  test_error_priority_bad_params();
  test_error_destroy_bad_params();
  test_getters();
  test_error_to_daemon_json();
}
