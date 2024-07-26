/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "tlib_main.h"
#include "nr_distributed_trace.h"
#include "nr_txn.h"
#include "nr_distributed_trace_private.h"
#include "util_memory.h"
#include <locale.h>

static void test_distributed_trace_create_destroy(void) {
  // create a few instances to make sure state stays separate
  // and destroy them to make sure any *alloc-y bugs are
  // caught by valgrind
  nr_distributed_trace_t* dt1;
  nr_distributed_trace_t* dt2;
  nr_distributed_trace_t* null_dt = NULL;

  dt1 = nr_distributed_trace_create();
  dt2 = nr_distributed_trace_create();

  nr_distributed_trace_set_sampled(dt1, true);
  nr_distributed_trace_set_sampled(dt2, false);

  tlib_pass_if_true("Set sampled to true", nr_distributed_trace_is_sampled(dt1),
                    "Expected true, got false");

  tlib_pass_if_false("Set sampled to false",
                     nr_distributed_trace_is_sampled(dt2),
                     "Expected false, got true");

  nr_distributed_trace_destroy(&dt1);
  nr_distributed_trace_destroy(&dt2);
  nr_distributed_trace_destroy(&null_dt);
}

static void test_distributed_trace_field_account_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_get_account_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value", nr_distributed_trace_get_account_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_app_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_get_app_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value", nr_distributed_trace_get_app_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_txn_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_get_txn_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value", nr_distributed_trace_get_txn_id(dt));

  /*
   * Test : Set value.
   */
  nr_distributed_trace_set_txn_id(dt, "txn_id");
  tlib_pass_if_str_equal("set txn_id", "txn_id",
                         nr_distributed_trace_get_txn_id(dt));

  /*
   * Test : Unset value.
   */
  nr_distributed_trace_set_txn_id(dt, NULL);
  tlib_pass_if_null("unset txn_id", nr_distributed_trace_get_txn_id(dt));

  /*
   * Test : Changed value.
   */
  nr_distributed_trace_set_txn_id(dt, "a");
  nr_distributed_trace_set_txn_id(dt, "b");
  tlib_pass_if_str_equal("changed txn_id", "b",
                         nr_distributed_trace_get_txn_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_priority(void) {
  nr_distributed_trace_t dt = {.priority = 0.0};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_double_equal("NULL dt", NR_PRIORITY_ERROR,
                            nr_distributed_trace_get_priority(NULL));

  /*
   * Test : Set value.
   */
  nr_distributed_trace_set_priority(&dt, 0.5);
  tlib_pass_if_double_equal("set priority", 0.5,
                            nr_distributed_trace_get_priority(&dt));

  /*
   * Test : Changed value.
   */
  nr_distributed_trace_set_priority(&dt, 0.8);
  tlib_pass_if_double_equal("set priority", 0.8,
                            nr_distributed_trace_get_priority(&dt));
}

static void test_distributed_trace_field_sampled(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  // test null cases
  tlib_pass_if_false("Return value for NULL pointer is false",
                     nr_distributed_trace_is_sampled(NULL),
                     "Expected false, got true");

  nr_distributed_trace_set_sampled(dt, true);
  nr_distributed_trace_set_sampled(NULL, false);

  tlib_pass_if_true("Value remains set after NULL pointer",
                    nr_distributed_trace_is_sampled(dt),
                    "Expected true, got false");

  // null case for destroy to make sure nothing explodes
  nr_distributed_trace_destroy(NULL);

  // test setting values back and forth
  nr_distributed_trace_set_sampled(dt, false);
  tlib_pass_if_false("Set sampled to false",
                     nr_distributed_trace_is_sampled(dt),
                     "Expected false, got true");

  nr_distributed_trace_set_sampled(dt, true);
  tlib_pass_if_true("Set sampled to true", nr_distributed_trace_is_sampled(dt),
                    "Expected true, got false");

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_trace_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_get_trace_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value", nr_distributed_trace_get_trace_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_tracing_vendors(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt",
                    nr_distributed_trace_inbound_get_tracing_vendors(NULL));
  tlib_pass_if_null("NULL tracingVendors list",
                    nr_distributed_trace_inbound_get_tracing_vendors(dt));

  nr_distributed_trace_inbound_set_tracing_vendors(dt, NULL);
  nr_distributed_trace_inbound_set_tracing_vendors(NULL, "tracingVendors");
  tlib_pass_if_null("the tracingVendors list should still be NULL",
                    nr_distributed_trace_inbound_get_tracing_vendors(dt));

  /*
   * Test : get && set value.
   */
  nr_distributed_trace_inbound_set_tracing_vendors(dt, "tracingVendors1");
  tlib_pass_if_str_equal("should be the first tracingVendors list we set",
                         "tracingVendors1",
                         nr_distributed_trace_inbound_get_tracing_vendors(dt));
  nr_distributed_trace_inbound_set_tracing_vendors(dt, "tracingVendors2");
  tlib_pass_if_str_equal("should be the second tracingVendors list we set",
                         "tracingVendors2",
                         nr_distributed_trace_inbound_get_tracing_vendors(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_trusted_parent_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt",
                    nr_distributed_trace_inbound_get_trusted_parent_id(NULL));
  tlib_pass_if_null("NULL trustedParentId",
                    nr_distributed_trace_inbound_get_trusted_parent_id(dt));

  nr_distributed_trace_inbound_set_trusted_parent_id(dt, NULL);
  nr_distributed_trace_inbound_set_trusted_parent_id(NULL, "trustedParentId");
  tlib_pass_if_null("the trustedParentId should still be NULL",
                    nr_distributed_trace_inbound_get_trusted_parent_id(dt));

  /*
   * Test : get && set value.
   */
  nr_distributed_trace_inbound_set_trusted_parent_id(dt, "trustedParentId1");
  tlib_pass_if_str_equal(
      "should be the first trustedParentId we set", "trustedParentId1",
      nr_distributed_trace_inbound_get_trusted_parent_id(dt));
  nr_distributed_trace_inbound_set_trusted_parent_id(dt, "trustedParentId2");
  tlib_pass_if_str_equal(
      "should be the second trustedParentId we set", "trustedParentId2",
      nr_distributed_trace_inbound_get_trusted_parent_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_type(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_inbound_get_type(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value", nr_distributed_trace_inbound_get_type(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_app_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_inbound_get_app_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value",
                    nr_distributed_trace_inbound_get_app_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_account_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt",
                    nr_distributed_trace_inbound_get_account_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value",
                    nr_distributed_trace_inbound_get_account_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_transport_type(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt",
                    nr_distributed_trace_inbound_get_transport_type(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value",
                    nr_distributed_trace_inbound_get_transport_type(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_timestamp_delta(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_long_equal(
      "NULL dt", 0, nr_distributed_trace_inbound_get_timestamp_delta(NULL, 0));

  /*
   * Test : Default value.
   */
  tlib_pass_if_long_equal(
      "default value", 0,
      nr_distributed_trace_inbound_get_timestamp_delta(dt, 0));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_has_timestamp(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false(
          "NULL dt", nr_distributed_trace_inbound_has_timestamp(NULL), "Expected true, got false");

  /*
   * Test : Default value.
   */
  tlib_pass_if_false(
          "default value",
          nr_distributed_trace_inbound_has_timestamp(dt), "Expected true, got false");

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_guid(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_inbound_get_guid(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value", nr_distributed_trace_inbound_get_guid(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_field_inbound_txn_id(void) {
  nr_distributed_trace_t* dt;

  dt = nr_distributed_trace_create();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dt", nr_distributed_trace_inbound_get_txn_id(NULL));

  /*
   * Test : Default value.
   */
  tlib_pass_if_null("default value",
                    nr_distributed_trace_inbound_get_txn_id(dt));

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_payload_txn_payload_timestamp_delta(void) {
  nr_distributed_trace_t* dt;
  nrtime_t payload_timestamp_ms = 1529445826000;
  nrtime_t txn_timestamp_us = 15214458260000 * NR_TIME_DIVISOR_MS;
  nrtime_t delta_timestamp_us = nr_time_duration(
      (payload_timestamp_ms * NR_TIME_DIVISOR_MS), txn_timestamp_us);

  const char* error = NULL;
  nrobj_t* obj_payload;

  char* json
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1529445826000 \
    } \
  }";

  obj_payload = nro_create_from_json(json);

  dt = nr_distributed_trace_create();
  nr_distributed_trace_accept_inbound_payload(dt, obj_payload, "HTTP", &error);
  tlib_pass_if_null("No error", error);

  tlib_fail_if_int64_t_equal("Zero duration", 0, delta_timestamp_us);
  tlib_pass_if_long_equal(
      "Compare payload and txn time", delta_timestamp_us,
      nr_distributed_trace_inbound_get_timestamp_delta(dt, txn_timestamp_us));

  nr_distributed_trace_destroy(&dt);

  nro_delete(obj_payload);
}

static void test_distributed_trace_payload_create_destroy(void) {
  nr_distributed_trace_t* dt;

  nr_distributed_trace_payload_t* payload1;
  nr_distributed_trace_payload_t* payload2;
  nr_distributed_trace_payload_t* null_payload = NULL;

  dt = nr_distributed_trace_create();

  payload1 = nr_distributed_trace_payload_create(NULL, "1234");
  payload2 = nr_distributed_trace_payload_create(dt, NULL);

  // null case for destroy to make sure nothing explodes
  nr_distributed_trace_payload_destroy(NULL);
  nr_distributed_trace_payload_destroy(&null_payload);

  tlib_pass_if_true(
      "parent_id is set correctly",
      nr_strcmp(nr_distributed_trace_payload_get_parent_id(payload1), "1234")
          == 0,
      "Expected true, got false");

  tlib_pass_if_true("Distributed metadata is set correctly",
                    dt == nr_distributed_trace_payload_get_metadata(payload2),
                    "Expected true, got false");

  nr_distributed_trace_payload_destroy(&payload1);
  nr_distributed_trace_payload_destroy(&payload2);

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_convert_payload_to_object(void) {
  const char* error = NULL;
  char* json = NULL;
  nrobj_t* payload = NULL;
  char* payload_string;

  // NULL Payload
  nr_distributed_trace_convert_payload_to_object(NULL, &error);
  tlib_pass_if_str_equal(
      "Empty DT", "Supportability/DistributedTrace/AcceptPayload/Ignored/Null",
      error);

  // Non-null error passed in (make sure it doesn't get overridden)
  error = "ZipZap";
  nr_distributed_trace_convert_payload_to_object(NULL, &error);
  tlib_pass_if_str_equal("Non-null Error", "ZipZap", error);
  error = NULL;

  // Invalid JSON
  nr_distributed_trace_convert_payload_to_object("Invalid json", &error);
  tlib_pass_if_str_equal(
      "Invalid payload",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  error = NULL;

  // Missing version
  json = nr_strdup(
      "{ \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Missing version",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
  error = NULL;

  // Incompatible major version
  json = nr_strdup(
      "{ \
    \"v\": [1,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Major version too high",
      "Supportability/DistributedTrace/AcceptPayload/Ignored/MajorVersion",
      error);
  nr_free(json);
  error = NULL;

  // Missing required key: Type
  json = nr_strdup(
      "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Missing required key: Type",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
  error = NULL;

  // Missing required key: Account ID
  json = nr_strdup(
      "{ \
    \"v\": [0,9],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Missing required key: Account ID",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
  error = NULL;

  // Missing required key: Application ID
  json = nr_strdup(
      "{ \
      \"v\": [0,1],   \
      \"d\": {        \
        \"ty\": \"App\", \
        \"ac\": \"9123\", \
        \"id\": \"27856f70d3d314b7\", \
        \"tr\": \"3221bf09aa0bcf0d\", \
        \"pr\": 0.1234, \
        \"sa\": false, \
        \"ti\": 1482959525577 \
      } \
    }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Missing required key: Application ID",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
  error = NULL;

  // Missing BOTH txn_id AND guid
  json = nr_strdup(
      "{ \
      \"v\": [0,1],   \
      \"d\": {        \
        \"ty\": \"App\", \
        \"ac\": \"9123\", \
        \"ap\": \"51424\", \
        \"tr\": \"3221bf09aa0bcf0d\", \
        \"pr\": 0.1234, \
        \"sa\": true, \
        \"ti\": 1482959525577 \
      } \
    }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Inbound distributed trace must have either d.tx or d.id, missing both",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
  error = NULL;

  // Missing txn_id, guid present
  json = nr_strdup(
      "{"
      "\"v\":[0,1],"
      "\"d\":{"
      "\"ty\":\"App\","
      "\"ac\":\"9123\","
      "\"ap\":\"51424\","
      "\"id\":\"14a8b295952a55f7\","
      "\"tr\":\"3221bf09aa0bcf0d\","
      "\"pr\":0.12340,"
      "\"sa\":true,"
      "\"ti\":1482959525577"
      "}"
      "}");
  payload = nr_distributed_trace_convert_payload_to_object(json, &error);
  payload_string = nro_to_json(payload);
  tlib_pass_if_null("there should not be errors", error);
  tlib_pass_if_not_null("the payload should not be null", payload);
  tlib_pass_if_str_equal("The payload object should equal the json object",
                         json, payload_string);
  nr_free(json);
  nr_free(payload_string);
  nro_real_delete(&payload);
  error = NULL;

  // txn_id present, Missing guid
  json = nr_strdup(
      "{"
      "\"v\":[0,1],"
      "\"d\":{"
      "\"ty\":\"App\","
      "\"ac\":\"9123\","
      "\"ap\":\"51424\","
      "\"tr\":\"3221bf09aa0bcf0d\","
      "\"pr\":0.12340,"
      "\"sa\":true,"
      "\"ti\":1482959525577,"
      "\"tx\":\"14a8b295952a55f7\""
      "}"
      "}");
  payload = nr_distributed_trace_convert_payload_to_object(json, &error);
  payload_string = nro_to_json(payload);
  tlib_pass_if_null("there should not be errors", error);
  tlib_pass_if_not_null("the payload should not be null", payload);
  tlib_pass_if_str_equal("The payload object should equal the json object",
                         json, payload_string);
  nr_free(json);
  nr_free(payload_string);
  nro_real_delete(&payload);
  error = NULL;

  // Missing required key: Trace ID
  json = nr_strdup(
      "{ \
      \"v\": [0,1],   \
      \"d\": {        \
        \"ty\": \"App\", \
        \"ac\": \"9123\", \
        \"ap\": \"51424\", \
        \"id\": \"27856f70d3d314b7\", \
        \"pr\": 0.1234, \
        \"sa\": false, \
        \"ti\": 1482959525577 \
      } \
    }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Missing required key: Trace ID",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
  error = NULL;

  // Missing required key: Timestamp
  json = nr_strdup(
      "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false \
    } \
  }");
  nr_distributed_trace_convert_payload_to_object(json, &error);
  tlib_pass_if_str_equal(
      "Missing required key: Timestamp",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  nr_free(json);
}

static void test_distributed_trace_payload_accept_inbound_payload(void) {
  nr_distributed_trace_t* dt;
  nrtime_t payload_timestamp_ms = 1482959525577;
  nrtime_t txn_timestamp_us
      = (payload_timestamp_ms * NR_TIME_DIVISOR_MS) - 100000000;

  const char* error = NULL;
  nrobj_t* obj_payload;

  char* json
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"tx\": \"6789\", \
      \"id\": \"4321\", \
      \"tk\": \"1010\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }";

  obj_payload = nro_create_from_json(json);

  dt = nr_distributed_trace_create();

  /*
   * Test : Null payload
   */
  tlib_pass_if_false(
      "Null payload",
      nr_distributed_trace_accept_inbound_payload(dt, NULL, "", &error),
      "Expected false");
  tlib_pass_if_str_equal(
      "Null payload",
      "Supportability/DistributedTrace/AcceptPayload/ParseException", error);
  error = NULL;

  /*
   * Test : Null DT
   */
  tlib_pass_if_false("Null dt",
                     nr_distributed_trace_accept_inbound_payload(
                         NULL, obj_payload, "", &error),
                     "Expected false");
  tlib_pass_if_str_equal(
      "Null dt", "Supportability/DistributedTrace/AcceptPayload/Exception",
      error);
  error = NULL;

  /*
   * Test : Successful
   */
  tlib_pass_if_true("Inbound processed",
                    nr_distributed_trace_accept_inbound_payload(
                        dt, obj_payload, "Other", &error),
                    "Expected NULL");
  tlib_pass_if_null("No supportability metric error thrown", error);
  tlib_pass_if_str_equal("Type", "App",
                         nr_distributed_trace_inbound_get_type(dt));
  tlib_pass_if_str_equal("Application ID", "51424",
                         nr_distributed_trace_inbound_get_app_id(dt));
  tlib_pass_if_str_equal("Account ID", "9123",
                         nr_distributed_trace_inbound_get_account_id(dt));
  tlib_pass_if_str_equal("Event Parent", "4321",
                         nr_distributed_trace_inbound_get_guid(dt));
  tlib_pass_if_str_equal("Transaction ID", "6789",
                         nr_distributed_trace_inbound_get_txn_id(dt));
  tlib_pass_if_str_equal("Transport Type", "Other",
                         nr_distributed_trace_inbound_get_transport_type(dt));
  tlib_pass_if_uint_equal(
      "Timestamp",
      nr_time_duration((payload_timestamp_ms * NR_TIME_DIVISOR_MS),
                       txn_timestamp_us),
      nr_distributed_trace_inbound_get_timestamp_delta(dt, txn_timestamp_us));

  nr_distributed_trace_destroy(&dt);
  nro_delete(obj_payload);
}

static void test_distributed_trace_payload_as_text(void) {
  nr_distributed_trace_t dt = {.priority = 0.5};
  nr_distributed_trace_payload_t payload
      = {.metadata = NULL, .parent_id = NULL, .timestamp = 60000};
  char* text;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL payload", nr_distributed_trace_payload_as_text(NULL));
  tlib_pass_if_null("NULL trace",
                    nr_distributed_trace_payload_as_text(&payload));

  /*
   * Test : Missing parent id and transaction id.
   */
  payload.metadata = &dt;
  tlib_pass_if_null("NULL payload", nr_distributed_trace_payload_as_text(NULL));
  tlib_pass_if_null("NULL trace",
                    nr_distributed_trace_payload_as_text(&payload));

  /*
   * Test : Valid payload, with all nullable fields NULL including the parent
   *        id.
   */
  payload.metadata = &dt;
  dt.txn_id = "txnid";
  text = nr_distributed_trace_payload_as_text(&payload);
  tlib_pass_if_str_equal("NULL fields",
                         "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"tx\":\"txnid\","
                         "\"pr\":0.50000,\"sa\":false,\"ti\":60}}",
                         text);
  nr_free(text);

  /*
   * Test : Valid payload, with all nullable fields NULL except for the parent
   *        id.
   */
  payload.metadata = &dt;
  payload.parent_id = "guid";
  dt.txn_id = NULL;
  text = nr_distributed_trace_payload_as_text(&payload);
  tlib_pass_if_str_equal("NULL fields",
                         "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"id\":\"guid\","
                         "\"pr\":0.50000,\"sa\":false,\"ti\":60}}",
                         text);
  nr_free(text);

  /*
   * Test : Valid payload, with all fields set.
   */
  dt.account_id = "account";
  dt.app_id = "app";
  dt.trace_id = "trace";
  dt.trusted_key = "tkey";
  dt.txn_id = "txnid";
  text = nr_distributed_trace_payload_as_text(&payload);
  tlib_pass_if_str_equal(
      "set fields",
      "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"ac\":\"account\",\"ap\":\"app\","
      "\"id\":\"guid\",\"tr\":\"trace\",\"tx\":\"txnid\",\"pr\":0.50000,"
      "\"sa\":false,\"ti\":60,\"tk\":\"tkey\"}}",
      text);
  nr_free(text);

  /*
   * Test : Valid payload, trusted key matches account id
   */
  dt.account_id = "account";
  dt.app_id = "app";
  dt.trace_id = "trace";
  dt.trusted_key = "account";
  text = nr_distributed_trace_payload_as_text(&payload);
  tlib_pass_if_str_equal(
      "set fields",
      "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"ac\":\"account\",\"ap\":\"app\","
      "\"id\":\"guid\",\"tr\":\"trace\",\"tx\":\"txnid\",\"pr\":0.50000,"
      "\"sa\":false,\"ti\":60}}",
      text);
  nr_free(text);
}

static void test_distributed_trace_convert_w3c_traceparent_invalid(void) {
  nrobj_t* res;
  const char* error = NULL;
  size_t index;
  struct testcase_t {
    const char* traceparent;
    const char* message;
  } testcases[] = {
      {NULL, "NULL trace parent"},
      {"00-22222222222222222222222222222222-3333333333333333",
       "too few trace parent fields"},
      {"00-222222222222222A2222222222222222-3333333333333333-01",
       "invalid characters"},
      {"00-22222222222222222222222222222222-33333333333?3333-01",
       "invalid characters"},
      {"---22222222222222222222222222222222-3333333333333333-01",
       "invalid characters"},
      {"00----------------------------------3333333333333333-01",
       "invalid characters"},
      {"00-22222222222222222222222222222222------------------01",
       "invalid characters"},
      {"00-2222222222222222222222222222222-3333333333333333---",
       "invalid characters"},
      {"00-222222222222222222222222222222-3333333333333333-01",
       "too short ids"},
      {"00-22222222222222222222222222222222-333333333333-01", "too short ids"},
      {"00-00000000000000000000000000000000-3333333333333333-01",
       "all zero trace id"},
      {"00-22222222222222222222222222222222-0000000000000000-01",
       "all zero parent id"},
      {"ff-22222222222222222222222222222222-3333333333333333-01",
       "invalid version"},
  };

  /*
   * Fail for parsing invalid trace parent headers.
   */
  for (index = 0; index < sizeof(testcases) / sizeof(testcases[0]); index++) {
    error = NULL;
    res = nr_distributed_trace_convert_w3c_headers_to_object(
        testcases[index].traceparent, NULL, NULL, &error);
    tlib_pass_if_null(testcases[index].message, res);
    tlib_pass_if_str_equal(
        testcases[index].message,
        "Supportability/TraceContext/TraceParent/Parse/Exception", error);
  }
}

static void test_distributed_trace_convert_w3c_traceparent(void) {
  nrobj_t* res;
  char* res_str;
  const char* error = NULL;

  /*
   * Parse a valid trace parent header into a nrobj_t.
   */
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01", NULL, NULL,
      &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("valid traceparent header", res);
  tlib_pass_if_str_equal("valid traceparent header", error,
                         "Supportability/TraceContext/TraceState/NoNrEntry");
  tlib_pass_if_str_equal("valid traceparent header", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "}"
                         "}");

  nro_delete(res);
  nr_free(res_str);
}

static void test_distributed_trace_convert_w3c_tracestate_invalid(void) {
  nrobj_t* res;
  const char* error = NULL;
  size_t index;
  struct testcase_t {
    const char* tracestate;
    const char* trusted_account_key;
    const char* message;
    const char* metric;
  } testcases[] = {
      {NULL, NULL, "NULL trace state",
       "Supportability/TraceContext/TraceState/NoNrEntry"},
      {"", "190", "empty trace state",
       "Supportability/TraceContext/TraceState/NoNrEntry"},
      {"190@nr=0-0-70-85-f8-16-1-0.789-1563", NULL, "NULL trusted account key",
       "Supportability/TraceContext/TraceState/NoNrEntry"},
      {"190@nr=0-0-70-85-f8-16-1-0.789", "190", "too few trace state fields",
       "Supportability/TraceContext/TraceState/InvalidNrEntry"},
      {"23@nr=0-0-70-85-f8-16-1-0.789-1563", "190",
       "different trusted account key",
       "Supportability/TraceContext/TraceState/NoNrEntry"},
  };

  /*
   * Fail for parsing invalid trace state headers.
   */
  for (index = 0; index < sizeof(testcases) / sizeof(testcases[0]); index++) {
    error = NULL;
    res = nr_distributed_trace_convert_w3c_headers_to_object(
        "00-22222222222222222222222222222222-3333333333333333-01",
        testcases[index].tracestate, testcases[index].trusted_account_key,
        &error);
    tlib_pass_if_not_null(testcases[index].message, res);
    tlib_pass_if_str_equal(testcases[index].message, testcases[index].metric,
                           error);
    nro_delete(res);
  }
}

static void test_distributed_trace_convert_w3c_tracestate(void) {
  nrobj_t* res;
  char* res_str;
  const char* error = NULL;

  /*
   * No NR entry.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "other=other,33@nr=other2", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("no NR entry", res);
  tlib_pass_if_str_equal(
      "no NR entry", "Supportability/TraceContext/TraceState/NoNrEntry", error);
  tlib_pass_if_str_equal("no NR entry", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracingVendors\":\"other,33@nr\","
                         "\"rawTracingVendors\":\"other=other,33@nr=other2\""
                         "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * Bad other vendors.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "other=other,33@nrother2, bad-header-no-equals", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("no NR entry", res);
  tlib_pass_if_str_equal(
      "no NR entry", "Supportability/TraceContext/TraceState/NoNrEntry", error);
  tlib_pass_if_str_equal(
      "no NR entry", res_str,
      "{"
      "\"traceparent\":{"
      "\"version\":\"00\","
      "\"trace_id\":\"22222222222222222222222222222222\","
      "\"parent_id\":\"3333333333333333\","
      "\"trace_flags\":1"
      "},"
      "\"tracingVendors\":\"other,33@nrother2,bad-header-no-equals\","
      "\"rawTracingVendors\":\"other=other,33@nrother2,bad-header-no-equals\""
      "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * Only required fields.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "190@nr=0-0-70-85-----1563", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("required fields set", res);
  tlib_pass_if_str_equal("required fields set", NULL, error);
  tlib_pass_if_str_equal("required fields set", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracestate\":{"
                         "\"version\":0,"
                         "\"parent_type\":0,"
                         "\"parent_account_id\":\"70\","
                         "\"parent_application_id\":\"85\","
                         "\"timestamp\":1563"
                         "}"
                         "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * All fields set.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "190@nr=0-0-70-85-4a3f-9eff-1-.342-1563", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("all fields set", res);
  tlib_pass_if_str_equal("all fields set", NULL, error);
  tlib_pass_if_str_equal("all fields set", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracestate\":{"
                         "\"version\":0,"
                         "\"parent_type\":0,"
                         "\"parent_account_id\":\"70\","
                         "\"parent_application_id\":\"85\","
                         "\"span_id\":\"4a3f\","
                         "\"transaction_id\":\"9eff\","
                         "\"sampled\":1,"
                         "\"priority\":0.34200,"
                         "\"timestamp\":1563"
                         "}"
                         "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * All fields set, other entries present.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "190@nr=0-0-70-85-4a3f-9eff-1-.342-1563,other=other,other2=other2", "190",
      &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("NR entry and other entries", res);
  tlib_pass_if_str_equal("NR entry and other entries", NULL, error);
  tlib_pass_if_str_equal("NR entry and other entries", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracingVendors\":\"other,other2\","
                         "\"rawTracingVendors\":\"other=other,other2=other2\","
                         "\"tracestate\":{"
                         "\"version\":0,"
                         "\"parent_type\":0,"
                         "\"parent_account_id\":\"70\","
                         "\"parent_application_id\":\"85\","
                         "\"span_id\":\"4a3f\","
                         "\"transaction_id\":\"9eff\","
                         "\"sampled\":1,"
                         "\"priority\":0.34200,"
                         "\"timestamp\":1563"
                         "}"
                         "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * Invalid NR entry, other entries present.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "190@nr=0,other=other,other2=other2", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("invalid NR entry other entries present", res);
  tlib_pass_if_str_equal(
      "invalid NR entry, other entries present",
      "Supportability/TraceContext/TraceState/InvalidNrEntry", error);
  tlib_pass_if_str_equal("invalid NR entry, other entries present", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracingVendors\":\"other,other2\","
                         "\"rawTracingVendors\":\"other=other,other2=other2\""
                         "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * An invalid priority should be omitted..
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "190@nr=0-0-70-85----1.2.3-1563", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("required fields set", res);
  tlib_pass_if_str_equal("required fields set", NULL, error);
  tlib_pass_if_str_equal("required fields set", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracestate\":{"
                         "\"version\":0,"
                         "\"parent_type\":0,"
                         "\"parent_account_id\":\"70\","
                         "\"parent_application_id\":\"85\","
                         "\"timestamp\":1563"
                         "}"
                         "}");

  nro_delete(res);
  nr_free(res_str);

  /*
   * A newer tracestate entry with additional fields. Those should be
   * ignored.
   */
  error = NULL;
  res = nr_distributed_trace_convert_w3c_headers_to_object(
      "00-22222222222222222222222222222222-3333333333333333-01",
      "190@nr=1-0-70-85-----1563-some-new-fields", "190", &error);
  res_str = nro_to_json(res);
  tlib_pass_if_not_null("newer tracestate version", res);
  tlib_pass_if_str_equal("newer tracestate version", NULL, error);
  tlib_pass_if_str_equal("newer tracestate version", res_str,
                         "{"
                         "\"traceparent\":{"
                         "\"version\":\"00\","
                         "\"trace_id\":\"22222222222222222222222222222222\","
                         "\"parent_id\":\"3333333333333333\","
                         "\"trace_flags\":1"
                         "},"
                         "\"tracestate\":{"
                         "\"version\":1,"
                         "\"parent_type\":0,"
                         "\"parent_account_id\":\"70\","
                         "\"parent_application_id\":\"85\","
                         "\"timestamp\":1563"
                         "}"
                         "}");

  nro_delete(res);
  nr_free(res_str);
}

static void test_create_trace_state_header(void) {
  nr_distributed_trace_t* dt = NULL;
  char* span_id = NULL;
  char* txn_id = "meatball!";
  char* expected = NULL;
  char* result;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null(
      "NULL dt & span should result in NULL header",
      nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, NULL));

  dt = nr_distributed_trace_create();

  span_id = "123456789";
  tlib_pass_if_null(
      "NULL dt should result in NULL header",
      nr_distributed_trace_create_w3c_tracestate_header(NULL, span_id, txn_id));

  nr_distributed_trace_set_sampled(dt, true);
  nr_distributed_trace_set_priority(dt, 0.234);

  tlib_pass_if_null(
      "NULL trusted key should result in NULL header",
      nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, txn_id));

  nr_distributed_trace_set_trusted_key(dt, "777");

  tlib_pass_if_null(
      "NULL account id should result in NULL header",
      nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, txn_id));

  nr_distributed_trace_set_account_id(dt, "1234");

  tlib_pass_if_null(
      "NULL app id should result in NULL header",
      nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, txn_id));

  nr_distributed_trace_set_app_id(dt, "9876");

  /*
   * Test : No span id or txn id
   */
  expected = "777@nr=0-0-1234-9876---1-0.234000-";
  result = nr_distributed_trace_create_w3c_tracestate_header(dt, NULL, NULL);
  tlib_pass_if_not_null(
      "NULL span id & txn id should result in a header w/o span id & txn id",
      nr_strstr(result, expected));
  nr_free(result);

  /*
   * Test : No span id
   */
  expected = "777@nr=0-0-1234-9876--meatball!-1-0.234000-";
  result = nr_distributed_trace_create_w3c_tracestate_header(dt, NULL, txn_id);
  tlib_pass_if_not_null("NULL span id should result in a header w/o span id",
                        nr_strstr(result, expected));
  nr_free(result);

  /*
   * Test : No txn id
   */
  nr_distributed_trace_set_sampled(dt, false);
  expected = "777@nr=0-0-1234-9876-123456789--0-0.234000-";
  result = nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, NULL);
  tlib_pass_if_not_null("NULL txn id should result in a header w/o txn id",
                        nr_strstr(result, expected));
  nr_free(result);

  /*
   * Test : Happy path
   */
  expected = "777@nr=0-0-1234-9876-123456789-meatball!-0-0.234000-";
  result
      = nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, txn_id);
  tlib_pass_if_not_null(
      "The trace context header did not match what was expected",
      nr_strstr(result, expected));
  nr_free(result);

  /*
   * Test : priority is rounded to 6 decimal places.
   */
  dt->priority = 0.123456789;
  expected = "777@nr=0-0-1234-9876-123456789-meatball!-0-0.123457-";
  result
      = nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, txn_id);
  tlib_pass_if_not_null("priority is rounded to 6 decimal places",
                        nr_strstr(result, expected));
  nr_free(result);

  /*
   * Test: locale is set to use `,` instead of `.` for decimal values
   */
  setlocale(LC_NUMERIC, "pl_PL");
  dt->priority = 0.123456;
  expected = "777@nr=0-0-1234-9876-123456789-meatball!-0-0.123456-";
  result
      = nr_distributed_trace_create_w3c_tracestate_header(dt, span_id, txn_id);
  tlib_pass_if_not_null("locale should not affect priority formatting", 
                        nr_strstr(result, expected));
  nr_free(result);
  setlocale(LC_NUMERIC, "en_US");

  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_accept_inbound_w3c_payload_invalid(void) {
  bool return_value = 1;
  const char* error = NULL;
  nrobj_t* trace_headers = NULL;
  nr_distributed_trace_t* dt = NULL;
  const char* transport_type = "HTTP";
  nrtime_t payload_timestamp_ms = 1529445826000;
  nrtime_t txn_timestamp_us = 15214458260000 * NR_TIME_DIVISOR_MS;
  nrtime_t delta_timestamp_us = nr_time_duration(
      (payload_timestamp_ms * NR_TIME_DIVISOR_MS), txn_timestamp_us);
  tlib_fail_if_int64_t_equal("Zero duration", 0, delta_timestamp_us);

  /*
   * Test : Everything is NULL
   */
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, NULL);
  tlib_pass_if_false("Everything is NULL", return_value, "return value = %d",
                     (int)return_value);

  /*
   * Test : Valid Error everything else is NULL
   */
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, NULL, &error);
  tlib_pass_if_false("Valid error", return_value, "return value = %d",
                     (int)return_value);
  tlib_pass_if_str_equal(
      "Everything else is NULL", error,
      NR_DISTRIBUTED_TRACE_W3C_TRACECONTEXT_ACCEPT_EXCEPTION);
  error = NULL;

  /*
   * Test : No DT object
   */
  trace_headers
      = nro_create_from_json("{\"tracestate\": {},\"traceparent\": {}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_false("No DT object", return_value, "return value = %d",
                     (int)return_value);
  tlib_pass_if_str_equal(
      "No DT object", error,
      NR_DISTRIBUTED_TRACE_W3C_TRACECONTEXT_ACCEPT_EXCEPTION);
  error = NULL;

  /*
   * Test : Valid dt, invalid objects
   */
  dt = nr_distributed_trace_create();
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, NULL, transport_type, &error);
  tlib_pass_if_false("No payloads", return_value, "return value = %d",
                     (int)return_value);
  tlib_pass_if_str_equal("No payloads", error,
                         NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION);
  error = NULL;

  /*
   * Test : NULL traceparent
   */
  nro_delete(trace_headers);
  trace_headers
      = nro_create_from_json("{\"traceparent\": {\"parentId\": \"chicken\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_false("NULL traceparent", return_value, "return value = %d",
                     (int)return_value);
  tlib_pass_if_str_equal("NULL traceparent", error,
                         NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION);
  error = NULL;

  /*
   * Test : No span Id's
   */
  nro_delete(trace_headers);
  trace_headers = nro_create_from_json(
      "{\"tracestate\": {\"something\": \"wombat\"}, \"traceparent\": "
      "{\"something\": \"chicken\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_false("No span id", return_value, "return value = %d",
                     (int)return_value);
  tlib_pass_if_str_equal("no span id", error,
                         NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION);
  error = NULL;

  /*
   * Test : No trace state span id
   */
  nro_delete(trace_headers);
  trace_headers = nro_create_from_json(
      "{\"tracestate\": {\"something\": \"wombat\"}, \"traceparent\": "
      "{\"parent_id\": \"spanIddd\", \"trace_id\": "
      "\"traceIdJson\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_null("only the traceparent span id is required", error);
  tlib_pass_if_str_equal("the dt parentId should be set", "spanIddd",
                         dt->inbound.guid);
  tlib_pass_if_true("only traceparent spanId", return_value,
                    "return value = %d", (int)return_value);
  tlib_pass_if_null("trusted parent should be NULL",
                    dt->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("The traceId should be set", "traceIdJson",
                         dt->trace_id);
  tlib_pass_if_null("The trusted be NULL", dt->inbound.trusted_parent_id);
  tlib_pass_if_null("The accountId should be NULL", dt->inbound.account_id);
  tlib_pass_if_null("The txn Id should be NULL", dt->inbound.txn_id);

  error = NULL;

  /*
   * Test : Different span id in tracestate and traceparent.
   */
  nro_delete(trace_headers);
  trace_headers = nro_create_from_json(
      "{\"tracestate\": {\"span_id\": \"wombat\", \"parent_account_id\": "
      "\"acc\", \"transaction_id\": \"txn\", \"sampled\": 1, \"priority\": "
      "0.1234, \"timestamp\": 1529445826000, \"parent_type\": 1}, "
      "\"tracingVendors\": \"dd,dt\", \"traceparent\": {\"parent_id\": "
      "\"spanIddd\", \"trace_id\": \"traceIdJson\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_null("Everything is set", error);
  tlib_pass_if_str_equal("the dt parentId should be set", "spanIddd",
                         dt->inbound.guid);
  tlib_pass_if_true("All values are set", return_value, "return value = %d",
                    (int)return_value);
  tlib_pass_if_str_equal("The trusted parent should come from the tracestate",
                         "wombat", dt->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("The accountId should have come from the tracestate",
                         "acc", dt->inbound.account_id);
  tlib_pass_if_str_equal("The txn Id should be set", "txn", dt->inbound.txn_id);
  tlib_pass_if_true("The sampled flag should be true", dt->sampled,
                    "sampled = %d", (int)dt->sampled);
  tlib_pass_if_double_equal("Priority should be set from tracestate", 0.1234,
                            dt->priority);
  tlib_pass_if_long_equal(
      "Compare payload and txn time", delta_timestamp_us,
      nr_distributed_trace_inbound_get_timestamp_delta(dt, txn_timestamp_us));
  tlib_pass_if_str_equal("Parent type should be set to Browser", "Browser",
                         dt->inbound.type);
  tlib_pass_if_str_equal("Other vendors should be populated", "dd,dt",
                         dt->inbound.tracing_vendors);
  error = NULL;
  nr_distributed_trace_destroy(&dt);

  /*
   * Test : Same span id in tracestate and traceparent.
   */
  nro_delete(trace_headers);
  dt = nr_distributed_trace_create();
  trace_headers = nro_create_from_json(
      "{\"tracestate\": {\"span_id\": \"spanIddd\", \"parent_account_id\": "
      "\"acc\", \"transaction_id\": \"txn\", \"sampled\": 1, \"priority\": "
      "0.1234, \"timestamp\": 1529445826000, \"parent_type\": 1}, "
      "\"tracingVendors\": \"dd,dt\", \"traceparent\": {\"parent_id\": "
      "\"spanIddd\", \"trace_id\": \"traceIdJson\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_null("Everything is set", error);
  tlib_pass_if_str_equal("the dt parentId should be set", "spanIddd",
                         dt->inbound.guid);
  tlib_pass_if_true("All values are set", return_value, "return value = %d",
                    (int)return_value);
  tlib_pass_if_str_equal("The trusted parent should come from the tracestate",
                         "spanIddd", dt->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("The accountId should have come from the tracestate",
                         "acc", dt->inbound.account_id);
  tlib_pass_if_str_equal("The txn Id should be set", "txn", dt->inbound.txn_id);
  tlib_pass_if_true("The sampled flag should be true", dt->sampled,
                    "sampled = %d", (int)dt->sampled);
  tlib_pass_if_double_equal("Priority should be set from tracestate", 0.1234,
                            dt->priority);
  tlib_pass_if_long_equal(
      "Compare payload and txn time", delta_timestamp_us,
      nr_distributed_trace_inbound_get_timestamp_delta(dt, txn_timestamp_us));
  tlib_pass_if_str_equal("Parent type should be set to Browser", "Browser",
                         dt->inbound.type);
  tlib_pass_if_str_equal("Other vendors should be populated", "dd,dt",
                         dt->inbound.tracing_vendors);
  error = NULL;
  nr_distributed_trace_destroy(&dt);

  /*
   * Test : different values
   */
  nro_delete(trace_headers);
  dt = nr_distributed_trace_create();
  trace_headers = nro_create_from_json(
      "{\"tracestate\": {\"span_id\": \"wombat\", \"sampled\": 0, "
      "\"priority\": 0.1234, \"timestamp\": 1529445826000, \"parent_type\": "
      "2}, \"tracingVendors\": \"dd,dt\", \"traceparent\": {\"parent_id\": "
      "\"spanIddd\", \"trace_id\": \"traceIdJson\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_null("All required values exist", error);
  tlib_pass_if_str_equal("the dt parentId should be set", "spanIddd",
                         dt->inbound.guid);
  tlib_pass_if_true("all required values exist", return_value,
                    "return value = %d", (int)return_value);
  tlib_pass_if_str_equal("The trusted parent should come from the tracestate",
                         "wombat", dt->inbound.trusted_parent_id);
  tlib_pass_if_false("The sampled flag should be false", dt->sampled,
                     "sampled = %d", (int)dt->sampled);
  tlib_pass_if_long_equal(
      "Compare payload and txn time", delta_timestamp_us,
      nr_distributed_trace_inbound_get_timestamp_delta(dt, txn_timestamp_us));
  tlib_pass_if_str_equal("Parent type should be set to Mobile", "Mobile",
                         dt->inbound.type);
  tlib_pass_if_null("The accountId should be NULL", dt->inbound.account_id);
  tlib_pass_if_null("The txn Id should be NULL", dt->inbound.txn_id);
  error = NULL;
  nr_distributed_trace_destroy(&dt);

  /*
   * Test : Valid traceparent with other vendor tracetate (no NR entry).
   */
  nro_delete(trace_headers);
  dt = nr_distributed_trace_create();
  trace_headers = nro_create_from_json(
      "{\"tracestate\": {}, "
      "\"tracingVendors\": \"foo,bar\", \"rawTracingVendors\": \"foo=1,bar=2\","
      "\"traceparent\": {\"parent_id\": \"spanId\", \"trace_id\": "
      "\"traceIdJson\"}}");
  return_value = nr_distributed_trace_accept_inbound_w3c_payload(
      dt, trace_headers, transport_type, &error);
  tlib_pass_if_null("Everything is set", error);
  tlib_pass_if_str_equal("the dt parentId should be set", "spanId",
                         dt->inbound.guid);
  tlib_pass_if_str_equal("the dt traceId should be set", "traceIdJson",
                         dt->trace_id);
  tlib_pass_if_true("All values are set", return_value, "return value = %d",
                    (int)return_value);
  tlib_pass_if_str_equal("Other vendors should be populated", "foo,bar",
                         dt->inbound.tracing_vendors);
  tlib_pass_if_str_equal(
      "The tracestate headers to be forwarded should be there", "foo=1,bar=2",
      dt->inbound.raw_tracing_vendors);

  nro_delete(trace_headers);
  nr_distributed_trace_destroy(&dt);
}

static void test_distributed_trace_create_trace_parent_header(void) {
  char* trace_id = "mEaTbAlLS";
  char* trace_id2 = "111122223333FoUrfIvE666677778888";
  char* long_trace_id = "111122223333FoUrfIvE6666777788889999";
  char* span_id = "currentspan";
  char* actual = NULL;
  char* expected = NULL;

  /*
   * Test : bad values
   */
  tlib_pass_if_null(
      "NULL trace id and span id",
      nr_distributed_trace_create_w3c_traceparent_header(NULL, NULL, false));

  tlib_pass_if_null(
      "NULL trace id, valid span",
      nr_distributed_trace_create_w3c_traceparent_header(NULL, span_id, false));

  tlib_pass_if_null(
      "NULL span id, valid trace id",
      nr_distributed_trace_create_w3c_traceparent_header(trace_id, NULL, true));

  /*
   * Test : valid values
   */
  actual = nr_distributed_trace_create_w3c_traceparent_header(trace_id, span_id,
                                                              true);
  expected = "00-00000000000000000000000meatballs-currentspan-01";
  tlib_pass_if_str_equal(
      "valid header with sampled is true and invalid trace_id", expected,
      actual);
  nr_free(actual);

  actual = nr_distributed_trace_create_w3c_traceparent_header(trace_id, span_id,
                                                              false);
  expected = "00-00000000000000000000000meatballs-currentspan-00";
  tlib_pass_if_str_equal(
      "valid header with sampled is false and invalid trace_id", expected,
      actual);
  nr_free(actual);

  actual = nr_distributed_trace_create_w3c_traceparent_header(trace_id2,
                                                              span_id, false);
  expected = "00-111122223333fourfive666677778888-currentspan-00";
  tlib_pass_if_str_equal("valid header with invalid trace_id", expected,
                         actual);
  nr_free(actual);

  actual = nr_distributed_trace_create_w3c_traceparent_header(long_trace_id,
                                                              span_id, false);
  expected = "00-111122223333fourfive666677778888-currentspan-00";
  tlib_pass_if_str_equal("valid header with invalid trace_id", expected,
                         actual);
  nr_free(actual);
}

static void test_distributed_trace_set_trace_id(const bool pad_trace_id) {
  nr_distributed_trace_t dt = {0};
  // clang-format off
  const char* t_input[] = {
      "",                  // empty string
      "1234567890",        // 10 characters
      "1234567890abcdef",  // 16 characters
      "1234567890123456789012345678901234567890123456789012345678901234567890"  // 64 characters
  };
  const char* t_padded[] = {
      "00000000000000000000000000000000",  // empty string lpadded to NR_TRACE_ID_MAX_SIZE with '0'
      "00000000000000000000001234567890",  // 10 characters lpadded to NR_TRACE_ID_MAX_SIZE with '0'
      "00000000000000001234567890abcdef",  // 16 characters lpadded to NR_TRACE_ID_MAX_SIZE with '0'
      "1234567890123456789012345678901234567890123456789012345678901234567890" // 64 characters - no padding
    };
  // clang-format on

  /*
   * Test : NULL input => no trace id generated
   */
  nr_distributed_trace_set_trace_id(&dt, NULL, pad_trace_id);
  tlib_pass_if_null("NULL trace id", dt.trace_id);

  /*
   * Test : valid input => trace id generated
   */
  for (long unsigned int i = 0; i < sizeof(t_input) / sizeof(t_input[0]); i++) {
    const char* expected = t_input[i];
    if (pad_trace_id) {
      expected = t_padded[i];
    }
    nr_distributed_trace_set_trace_id(&dt, t_input[i], pad_trace_id);
    tlib_pass_if_not_null("trace id is set", dt.trace_id);
    tlib_pass_if_str_equal("trace id has correct value", dt.trace_id, expected);
    nr_free(dt.trace_id);
  }
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_distributed_trace_create_destroy();
  test_distributed_trace_field_account_id();
  test_distributed_trace_field_app_id();
  test_distributed_trace_field_txn_id();
  test_distributed_trace_field_priority();
  test_distributed_trace_field_sampled();
  test_distributed_trace_field_trace_id();

  test_distributed_trace_field_inbound_type();
  test_distributed_trace_field_inbound_app_id();
  test_distributed_trace_field_inbound_account_id();
  test_distributed_trace_field_inbound_transport_type();
  test_distributed_trace_field_inbound_timestamp_delta();
  test_distributed_trace_field_inbound_has_timestamp();
  test_distributed_trace_field_inbound_guid();
  test_distributed_trace_field_inbound_txn_id();
  test_distributed_trace_field_inbound_tracing_vendors();
  test_distributed_trace_field_inbound_trusted_parent_id();

  test_distributed_trace_payload_txn_payload_timestamp_delta();

  test_distributed_trace_payload_create_destroy();
  test_distributed_trace_convert_payload_to_object();
  test_distributed_trace_payload_accept_inbound_payload();
  test_distributed_trace_payload_as_text();

  test_distributed_trace_convert_w3c_traceparent();
  test_distributed_trace_convert_w3c_traceparent_invalid();
  test_distributed_trace_convert_w3c_tracestate_invalid();
  test_distributed_trace_convert_w3c_tracestate();
  test_distributed_trace_accept_inbound_w3c_payload_invalid();

  test_create_trace_state_header();
  test_distributed_trace_create_trace_parent_header();
  test_distributed_trace_set_trace_id(false);
  test_distributed_trace_set_trace_id(true);
}
