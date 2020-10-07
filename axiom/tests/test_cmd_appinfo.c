/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_limits.h"
#include "nr_rules.h"
#include "util_buffer.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_network.h"
#include "util_reply.h"
#include "util_strings.h"

#include "tlib_main.h"
#include "test_app_helpers.h"

#define test_pass_if_empty_vector(T, I)  \
  tlib_pass_if_size_t_equal(__func__, 0, \
                            nr_flatbuffers_table_read_vector_len((T), (I)))

static void test_create_empty_query(void) {
  nr_flatbuffers_table_t msg, app;
  nr_app_info_t info;
  nr_flatbuffer_t* query;
  int high_security;

  nr_memset(&info, 0, sizeof(info));
  query = nr_appinfo_create_query("", "", &info);

  nr_flatbuffers_table_init_root(&msg, nr_flatbuffers_data(query),
                                 nr_flatbuffers_len(query));
  test_pass_if_empty_vector(&msg, MESSAGE_FIELD_AGENT_RUN_ID);

  nr_flatbuffers_table_read_union(&app, &msg, MESSAGE_FIELD_DATA);
  test_pass_if_empty_vector(&app, APP_FIELD_LICENSE);
  test_pass_if_empty_vector(&app, APP_FIELD_APPNAME);
  test_pass_if_empty_vector(&app, APP_FIELD_AGENT_LANGUAGE);
  test_pass_if_empty_vector(&app, APP_FIELD_AGENT_VERSION);
  test_pass_if_empty_vector(&app, APP_FIELD_REDIRECT_COLLECTOR);
  test_pass_if_empty_vector(&app, APP_FIELD_ENVIRONMENT);
  test_pass_if_empty_vector(&app, APP_FIELD_SETTINGS);
  test_pass_if_empty_vector(&app, APP_DISPLAY_HOST);
  test_pass_if_empty_vector(&app, APP_HOST);
  test_pass_if_empty_vector(&app, APP_TRACE_OBSERVER_HOST);
  test_pass_if_empty_vector(&app, APP_FIELD_LABELS);

  high_security
      = nr_flatbuffers_table_read_i8(&app, APP_FIELD_HIGH_SECURITY, 42);
  tlib_pass_if_false(__func__, 0 == high_security, "high_security=%d",
                     high_security);

  tlib_pass_if_uint16_t_equal(
      __func__, 0,
      nr_flatbuffers_table_read_u16(&app, APP_TRACE_OBSERVER_PORT, 0));

  tlib_pass_if_uint64_t_equal(
      __func__, 0, nr_flatbuffers_table_read_u64(&app, APP_SPAN_QUEUE_SIZE, 0));

  nr_flatbuffers_destroy(&query);
}

static void test_create_query(void) {
  nr_app_info_t info;
  nr_flatbuffers_table_t msg, app;
  nr_flatbuffer_t* query;
  int high_security;
  const char* settings_json = "[\"my_settings\"]";

  info.high_security = 1;
  info.license = nr_strdup("my_license");
  info.settings = nro_create_from_json(settings_json);
  info.environment = nro_create_from_json("{\"my_environment\":\"hi\"}");
  info.labels = nro_create_from_json("{\"my_labels\":\"hello\"}");
  info.host_display_name = nr_strdup("my_host_display_name");
  info.lang = nr_strdup("my_lang");
  info.version = nr_strdup("my_version");
  info.appname = nr_strdup("my_appname");
  info.redirect_collector = nr_strdup("my_redirect_collector");
  info.security_policies_token = nr_strdup("my_security_policy_token");
  info.supported_security_policies = nro_create_from_json("{\"foo\":false}");
  info.trace_observer_host = nr_strdup("my_trace_observer");
  info.trace_observer_port = 443;
  info.span_queue_size = 10000;

  query = nr_appinfo_create_query("12345", "this_host", &info);

  nr_flatbuffers_table_init_root(&msg, nr_flatbuffers_data(query),
                                 nr_flatbuffers_len(query));

  nr_flatbuffers_table_read_union(&app, &msg, MESSAGE_FIELD_DATA);

  tlib_pass_if_str_equal(
      __func__, info.license,
      (const char*)nr_flatbuffers_table_read_bytes(&app, APP_FIELD_LICENSE));
  tlib_pass_if_str_equal(
      __func__, info.appname,
      (const char*)nr_flatbuffers_table_read_bytes(&app, APP_FIELD_APPNAME));
  tlib_pass_if_str_equal(
      __func__, info.host_display_name,
      (const char*)nr_flatbuffers_table_read_bytes(&app, APP_DISPLAY_HOST));
  tlib_pass_if_str_equal(__func__, info.lang,
                         (const char*)nr_flatbuffers_table_read_bytes(
                             &app, APP_FIELD_AGENT_LANGUAGE));
  tlib_pass_if_str_equal(__func__, info.version,
                         (const char*)nr_flatbuffers_table_read_bytes(
                             &app, APP_FIELD_AGENT_VERSION));
  tlib_pass_if_str_equal(__func__, info.redirect_collector,
                         (const char*)nr_flatbuffers_table_read_bytes(
                             &app, APP_FIELD_REDIRECT_COLLECTOR));
  tlib_pass_if_str_equal(
      __func__, settings_json,
      (const char*)nr_flatbuffers_table_read_bytes(&app, APP_FIELD_SETTINGS));
  tlib_pass_if_str_equal(
      __func__, "[{\"label_type\":\"my_labels\",\"label_value\":\"hello\"}]",
      (const char*)nr_flatbuffers_table_read_bytes(&app, APP_FIELD_LABELS));
  tlib_pass_if_str_equal(__func__, "[[\"my_environment\",\"hi\"]]",
                         (const char*)nr_flatbuffers_table_read_bytes(
                             &app, APP_FIELD_ENVIRONMENT));
  tlib_pass_if_str_equal(
      __func__, "this_host",
      (const char*)nr_flatbuffers_table_read_bytes(&app, APP_HOST));
  tlib_pass_if_str_equal(__func__, info.trace_observer_host,
                         (const char*)nr_flatbuffers_table_read_bytes(
                             &app, APP_TRACE_OBSERVER_HOST));
  tlib_pass_if_uint16_t_equal(
      __func__, info.trace_observer_port,
      nr_flatbuffers_table_read_u16(&app, APP_TRACE_OBSERVER_PORT, 0));
  tlib_pass_if_uint64_t_equal(
      __func__, info.span_queue_size,
      nr_flatbuffers_table_read_u16(&app, APP_SPAN_QUEUE_SIZE, 0));

  high_security
      = nr_flatbuffers_table_read_i8(&app, APP_FIELD_HIGH_SECURITY, 0);
  tlib_pass_if_true(__func__, 1 == high_security, "high_security=%d",
                    high_security);

  nr_app_info_destroy_fields(&info);
  nr_flatbuffers_destroy(&query);
}

/* Create a faux reply from the Daemon by populating the flatbuffer object.
 * This is the two field version for unit testing against the legacy daemon
 * that existed prior to Language Agent Security Policy (LASP) implementation.
 * nr_cmd_appinfo_process_reply will handle the flatbuffer data.
 */
static nr_flatbuffer_t* create_app_reply_two_fields(const char* agent_run_id,
                                                    int8_t status,
                                                    const char* connect_json) {
  nr_flatbuffer_t* fb;
  uint32_t body;
  uint32_t agent_run_id_offset;
  uint32_t connect_json_offset;

  agent_run_id_offset = 0;
  connect_json_offset = 0;
  fb = nr_flatbuffers_create(0);

  if (connect_json && *connect_json) {
    connect_json_offset = nr_flatbuffers_prepend_string(fb, connect_json);
  }

  // This is set to a constant of `2` instead of the constant
  // APP_REPLY_NUM_FIELDS because this function is testing legacy functionality,
  // when there were only two fields of data in the flatbuffer.
  nr_flatbuffers_object_begin(fb, 2);
  nr_flatbuffers_object_prepend_i8(fb, APP_REPLY_FIELD_STATUS, status, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_REPLY_FIELD_CONNECT_REPLY,
                                        connect_json_offset, 0);
  body = nr_flatbuffers_object_end(fb);

  if (agent_run_id && *agent_run_id) {
    agent_run_id_offset = nr_flatbuffers_prepend_string(fb, agent_run_id);
  }

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, body, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_APP_REPLY, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id_offset, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  return fb;
}

/* Create a faux reply from the Daemon by populating the flatbuffer object.
 * This is the three field version for unit testing against the daemon version
 * updated to supported LASP. nr_cmd_appinfo_process_reply will handle the
 * flatbuffer data.
 */
static nr_flatbuffer_t* create_app_reply_three_fields(
    const char* agent_run_id,
    int8_t status,
    const char* connect_json,
    const char* security_policies) {
  nr_flatbuffer_t* fb;
  uint32_t body;
  uint32_t agent_run_id_offset;
  uint32_t connect_json_offset;
  uint32_t security_policies_offset;

  agent_run_id_offset = 0;
  connect_json_offset = 0;
  security_policies_offset = 0;
  fb = nr_flatbuffers_create(0);

  if (security_policies && *security_policies) {
    security_policies_offset
        = nr_flatbuffers_prepend_string(fb, security_policies);
  }

  if (connect_json && *connect_json) {
    connect_json_offset = nr_flatbuffers_prepend_string(fb, connect_json);
  }

  // This is set to a constant of `3` instead of the constant
  // APP_REPLY_NUM_FIELDS because this function is testing legacy functionality,
  // when there were only two fields of data in the flatbuffer.
  nr_flatbuffers_object_begin(fb, 3);
  nr_flatbuffers_object_prepend_i8(fb, APP_REPLY_FIELD_STATUS, status, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_REPLY_FIELD_CONNECT_REPLY,
                                        connect_json_offset, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_REPLY_FIELD_SECURITY_POLICIES,
                                        security_policies_offset, 0);
  body = nr_flatbuffers_object_end(fb);

  if (agent_run_id && *agent_run_id) {
    agent_run_id_offset = nr_flatbuffers_prepend_string(fb, agent_run_id);
  }

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, body, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_APP_REPLY, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id_offset, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  return fb;
}

/* Create a faux reply from the Daemon by populating the flatbuffer object.
 * This is the six field version for unit testing the case where the daemon
 * supports Distributed Tracing. nr_cmd_appinfo_process_reply will handle the
 * flatbuffer data. */
static nr_flatbuffer_t* create_app_reply_six_fields(
    const char* agent_run_id,
    int8_t status,
    const char* connect_json,
    const char* security_policies,
    nrtime_t connect_timestamp,
    uint16_t harvest_frequency,
    uint16_t sampling_target) {
  nr_flatbuffer_t* fb;
  uint32_t body;
  uint32_t agent_run_id_offset;
  uint32_t connect_json_offset;
  uint32_t security_policies_offset;

  agent_run_id_offset = 0;
  connect_json_offset = 0;
  security_policies_offset = 0;
  fb = nr_flatbuffers_create(0);

  if (security_policies && *security_policies) {
    security_policies_offset
        = nr_flatbuffers_prepend_string(fb, security_policies);
  }

  if (connect_json && *connect_json) {
    connect_json_offset = nr_flatbuffers_prepend_string(fb, connect_json);
  }

  nr_flatbuffers_object_begin(fb, APP_REPLY_NUM_FIELDS);
  nr_flatbuffers_object_prepend_i8(fb, APP_REPLY_FIELD_STATUS, status, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_REPLY_FIELD_CONNECT_REPLY,
                                        connect_json_offset, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, APP_REPLY_FIELD_SECURITY_POLICIES,
                                        security_policies_offset, 0);
  nr_flatbuffers_object_prepend_u64(fb, APP_REPLY_FIELD_CONNECT_TIMESTAMP,
                                    connect_timestamp, 0);
  nr_flatbuffers_object_prepend_u16(fb, APP_REPLY_FIELD_HARVEST_FREQUENCY,
                                    harvest_frequency, 0);
  nr_flatbuffers_object_prepend_u16(fb, APP_REPLY_FIELD_SAMPLING_TARGET,
                                    sampling_target, 0);
  body = nr_flatbuffers_object_end(fb);

  if (agent_run_id && *agent_run_id) {
    agent_run_id_offset = nr_flatbuffers_prepend_string(fb, agent_run_id);
  }

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, body, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_APP_REPLY, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id_offset, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  return fb;
}

static void test_process_null_reply(void) {
  nrapp_t app;
  nr_status_t st;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  st = nr_cmd_appinfo_process_reply(NULL, 0, &app);
  tlib_pass_if_status_failure(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
}

static void test_process_null_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply_two_fields(NULL, APP_STATUS_UNKNOWN, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), NULL);
  tlib_pass_if_status_failure(__func__, st);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_three_fields(NULL, APP_STATUS_UNKNOWN, NULL, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), NULL);
  tlib_pass_if_status_failure(__func__, st);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_six_fields(NULL, APP_STATUS_UNKNOWN, NULL, NULL, 1,
                                      2, 1);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), NULL);
  tlib_pass_if_status_failure(__func__, st);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_missing_body(void) {
  nr_flatbuffer_t* reply;
  nr_status_t st;
  uint32_t agent_run_id;

  reply = nr_flatbuffers_create(0);
  agent_run_id = nr_flatbuffers_prepend_string(reply, "12345");

  nr_flatbuffers_object_begin(reply, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(reply, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id, 0);
  nr_flatbuffers_finish(reply, nr_flatbuffers_object_end(reply));

  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), NULL);
  tlib_pass_if_status_failure(__func__, st);

  nr_flatbuffers_destroy(&reply);
}

static void test_process_wrong_body_type(void) {
  nr_flatbuffer_t* fb;
  nr_status_t st;
  uint32_t agent_run_id;
  uint32_t body;

  fb = nr_flatbuffers_create(0);

  nr_flatbuffers_object_begin(fb, TRANSACTION_NUM_FIELDS);
  body = nr_flatbuffers_object_end(fb);

  agent_run_id = nr_flatbuffers_prepend_string(fb, "12345");
  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, body, 0);
  nr_flatbuffers_object_prepend_i8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_TXN, 0);
  nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));

  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(fb),
                                    nr_flatbuffers_len(fb), NULL);
  tlib_pass_if_status_failure(__func__, st);

  nr_flatbuffers_destroy(&fb);
}

static void test_process_unknown_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;

  reply = create_app_reply_two_fields(NULL, APP_STATUS_UNKNOWN, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_three_fields(NULL, APP_STATUS_UNKNOWN, NULL, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_six_fields(NULL, APP_STATUS_UNKNOWN, NULL, NULL, 1,
                                      2, 1);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_invalid_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply_two_fields(NULL, APP_STATUS_INVALID_LICENSE, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_INVALID);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_three_fields(NULL, APP_STATUS_INVALID_LICENSE, NULL,
                                        NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_INVALID);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_six_fields(NULL, APP_STATUS_INVALID_LICENSE, NULL,
                                      NULL, 1, 2, 3);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_INVALID);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_disconnected_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;

  reply = create_app_reply_two_fields(NULL, APP_STATUS_DISCONNECTED, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_INVALID);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_three_fields(NULL, APP_STATUS_DISCONNECTED, NULL,
                                        NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_INVALID);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_six_fields(NULL, APP_STATUS_DISCONNECTED, NULL, NULL,
                                      1, 2, 3);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_INVALID);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_still_valid_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  reply = create_app_reply_two_fields(NULL, APP_STATUS_STILL_VALID, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  nr_flatbuffers_destroy(&reply);

  reply
      = create_app_reply_three_fields(NULL, APP_STATUS_STILL_VALID, NULL, NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_six_fields(NULL, APP_STATUS_STILL_VALID, NULL, NULL,
                                      1, 2, 3);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);

  // These fields should be ignored for an APP_STATUS_STILL_VALID reply
  tlib_pass_if_uint64_t_equal(__func__, 0, app.harvest.connect_timestamp);
  tlib_pass_if_uint64_t_equal(__func__, 0, app.harvest.frequency);
  tlib_pass_if_uint64_t_equal(__func__, 0,
                              app.harvest.target_transactions_per_cycle);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_connected_app_missing_json(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;
  const char* security_policies;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  security_policies
      = "{"
        "\"security_policies\": {"
        "\"record_sql\":"
        "{ \"enabled\": true, \"required\": false },"
        "\"custom_parameters\":"
        "{ \"enabled\": false, \"required\": false }"
        "}}";

  reply = create_app_reply_two_fields("346595271037263", APP_STATUS_CONNECTED,
                                      NULL);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_failure(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_three_fields("346595271037263", APP_STATUS_CONNECTED,
                                        NULL, security_policies);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_failure(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
  nr_flatbuffers_destroy(&reply);

  reply = create_app_reply_six_fields("346595271037263", APP_STATUS_CONNECTED,
                                      NULL, security_policies, 1, 2, 3);
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_failure(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_UNKNOWN);
  nr_flatbuffers_destroy(&reply);

  nro_delete(app.security_policies);
}

static void test_process_connected_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;
  const char* connect_json;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  connect_json
      = "{"
        "\"agent_run_id\":\"346595271037263\","
        "\"entity_guid\":\"00112233445566778899aa\","
        "\"url_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":"
        "false,"
        "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,"
        "\"replacement\":\"b\"}],"
        "\"transaction_name_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":"
        "false,"
        "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,"
        "\"replacement\":\"b\"}],"
        "\"transaction_segment_terms\":[{\"prefix\":\"Foo/"
        "Bar\",\"terms\":[\"a\",\"b\"]}],"
        "\"event_harvest_config\":{"
        "\"report_period_ms\":5000,"
        "\"harvest_limits\":{"
        "\"analytic_event_data\":833,"
        "\"custom_event_data\":0,"
        "\"error_event_data\":null"
        "}"
        "}"
        "}";

  reply = create_app_reply_two_fields("346595271037263", APP_STATUS_CONNECTED,
                                      connect_json);

  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal(__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_str_equal(__func__, app.entity_guid, "00112233445566778899aa");
  tlib_pass_if_not_null(__func__, app.connect_reply);
  tlib_pass_if_not_null(__func__, app.url_rules);
  tlib_pass_if_not_null(__func__, app.txn_rules);
  tlib_pass_if_not_null(__func__, app.segment_terms);

  /*
   * The harvest limits should turn into these event flags:
   *
   * 1. analytics_events_limit is 833 because the field is present and set to
   *    833.
   * 2. custom_events_limit is 0 because the field is present and set to 0.
   * 3. error_events_limit is 100 because the field is present but invalid,
   *    as it is null, so the default value is used.
   * 4. span_events_limit is 1000 because the field is omitted, so the default
   *    value is used.
   */
  tlib_pass_if_int_equal(__func__, 833, app.limits.analytics_events);
  tlib_pass_if_int_equal(__func__, 0, app.limits.custom_events);
  tlib_pass_if_int_equal(__func__, NR_MAX_ERRORS, app.limits.error_events);
  tlib_pass_if_int_equal(__func__, NR_MAX_SPAN_EVENTS, app.limits.span_events);

  /*
   * Perform same test again to make sure that populated fields are freed
   * before assignment.
   */
  app.state = NR_APP_UNKNOWN;
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal(__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null(__func__, app.connect_reply);
  tlib_pass_if_not_null(__func__, app.url_rules);
  tlib_pass_if_not_null(__func__, app.txn_rules);
  tlib_pass_if_not_null(__func__, app.segment_terms);

  nr_free(app.agent_run_id);
  nr_free(app.entity_guid);
  nro_delete(app.connect_reply);
  nr_rules_destroy(&app.url_rules);
  nr_rules_destroy(&app.txn_rules);
  nr_segment_terms_destroy(&app.segment_terms);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_lasp_connected_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;
  const char* connect_json;
  const char* security_policies;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  connect_json
      = "{"
        "\"agent_run_id\":\"346595271037263\","
        "\"url_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":"
        "false,"
        "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,"
        "\"replacement\":\"b\"}],"
        "\"transaction_name_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":"
        "false,"
        "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,"
        "\"replacement\":\"b\"}],"
        "\"transaction_segment_terms\":[{\"prefix\":\"Foo/"
        "Bar\",\"terms\":[\"a\",\"b\"]}]"
        "}";

  security_policies
      = "{"
        "\"record_sql\": true,"
        "\"custom_parameters\": false"
        "}";

  reply = create_app_reply_three_fields("346595271037263", APP_STATUS_CONNECTED,
                                        connect_json, security_policies);

  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal(__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null(__func__, app.connect_reply);
  tlib_pass_if_not_null(__func__, app.security_policies);
  tlib_pass_if_not_null(__func__, app.url_rules);
  tlib_pass_if_not_null(__func__, app.txn_rules);
  tlib_pass_if_not_null(__func__, app.segment_terms);

  /* Test the contents of security_policies to ensure the data was captured
   * correctly
   */
  tlib_pass_if_int_equal(
      __func__, nro_get_hash_boolean(app.security_policies, "record_sql", NULL),
      1);
  tlib_pass_if_int_equal(
      __func__,
      nro_get_hash_boolean(app.security_policies, "custom_parameters", NULL),
      0);

  /*
   * Perform same test again to make sure that populated fields are freed
   * before assignment.
   */
  app.state = NR_APP_UNKNOWN;
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal(__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null(__func__, app.connect_reply);
  tlib_pass_if_not_null(__func__, app.security_policies);
  tlib_pass_if_not_null(__func__, app.url_rules);
  tlib_pass_if_not_null(__func__, app.txn_rules);
  tlib_pass_if_not_null(__func__, app.segment_terms);

  nr_free(app.agent_run_id);
  nro_delete(app.connect_reply);
  nro_delete(app.security_policies);
  nr_rules_destroy(&app.url_rules);
  nr_rules_destroy(&app.txn_rules);
  nr_segment_terms_destroy(&app.segment_terms);
  nr_flatbuffers_destroy(&reply);
}

static void test_process_harvest_timing_connected_app(void) {
  nrapp_t app;
  nr_status_t st;
  nr_flatbuffer_t* reply;
  const char* connect_json;
  const char* security_policies;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_UNKNOWN;

  connect_json
      = "{"
        "\"agent_run_id\":\"346595271037263\","
        "\"url_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":"
        "false,"
        "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,"
        "\"replacement\":\"b\"}],"
        "\"transaction_name_rules\":"
        "[{\"each_segment\":false,\"terminate_chain\":true,\"replace_all\":"
        "false,"
        "\"match_expression\":\"^a$\",\"ignore\":false,\"eval_order\":0,"
        "\"replacement\":\"b\"}],"
        "\"transaction_segment_terms\":[{\"prefix\":\"Foo/"
        "Bar\",\"terms\":[\"a\",\"b\"]}]"
        "}";

  security_policies
      = "{"
        "\"record_sql\": true,"
        "\"custom_parameters\": false"
        "}";

  reply = create_app_reply_six_fields("346595271037263", APP_STATUS_CONNECTED,
                                      connect_json, security_policies, 1, 2, 3);

  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal(__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null(__func__, app.connect_reply);
  tlib_pass_if_not_null(__func__, app.security_policies);
  tlib_pass_if_not_null(__func__, app.url_rules);
  tlib_pass_if_not_null(__func__, app.txn_rules);
  tlib_pass_if_not_null(__func__, app.segment_terms);

  /* Test the contents of security_policies to ensure the data was captured
   * correctly
   */
  tlib_pass_if_int_equal(
      __func__, nro_get_hash_boolean(app.security_policies, "record_sql", NULL),
      1);
  tlib_pass_if_int_equal(
      __func__,
      nro_get_hash_boolean(app.security_policies, "custom_parameters", NULL),
      0);

  /* Test the harvest timing fields. */
  tlib_pass_if_uint64_t_equal(__func__, 1 * NR_TIME_DIVISOR,
                              app.harvest.connect_timestamp);
  tlib_pass_if_uint64_t_equal(__func__, 2 * NR_TIME_DIVISOR,
                              app.harvest.frequency);
  tlib_pass_if_uint64_t_equal(__func__, 3,
                              app.harvest.target_transactions_per_cycle);

  /*
   * Perform same test again to make sure that populated fields are freed
   * before assignment.
   */
  app.state = NR_APP_UNKNOWN;
  st = nr_cmd_appinfo_process_reply(nr_flatbuffers_data(reply),
                                    nr_flatbuffers_len(reply), &app);
  tlib_pass_if_status_success(__func__, st);
  tlib_pass_if_int_equal(__func__, (int)app.state, (int)NR_APP_OK);
  tlib_pass_if_str_equal(__func__, app.agent_run_id, "346595271037263");
  tlib_pass_if_not_null(__func__, app.connect_reply);
  tlib_pass_if_not_null(__func__, app.security_policies);
  tlib_pass_if_not_null(__func__, app.url_rules);
  tlib_pass_if_not_null(__func__, app.txn_rules);
  tlib_pass_if_not_null(__func__, app.segment_terms);

  nr_free(app.agent_run_id);
  nro_delete(app.connect_reply);
  nro_delete(app.security_policies);
  nr_rules_destroy(&app.url_rules);
  nr_rules_destroy(&app.txn_rules);
  nr_segment_terms_destroy(&app.segment_terms);
  nr_flatbuffers_destroy(&reply);
}

static nr_flatbuffer_t* create_app_reply_timing_flatbuffer(uint64_t timestamp,
                                                           uint16_t frequency) {
  nr_flatbuffer_t* fb = nr_flatbuffers_create(0);
  uint32_t reply;

  if (timestamp || frequency) {
    nr_flatbuffers_object_begin(fb, APP_NUM_FIELDS);
    nr_flatbuffers_object_prepend_u64(fb, APP_REPLY_FIELD_CONNECT_TIMESTAMP,
                                      timestamp, 0);
    nr_flatbuffers_object_prepend_u16(fb, APP_REPLY_FIELD_HARVEST_FREQUENCY,
                                      frequency, 0);
  } else {
    nr_flatbuffers_object_begin(fb, 3);
  }

  reply = nr_flatbuffers_object_end(fb);
  nr_flatbuffers_finish(fb, reply);

  return fb;
}

static void test_process_harvest_timing(void) {
  nrapp_t app = {.state = APP_STATUS_UNKNOWN};
  nr_flatbuffer_t* fb;
  nr_flatbuffers_table_t table;

  /*
   * Test : Both timestamp and frequency set.
   */
  fb = create_app_reply_timing_flatbuffer(1234, 56);
  nr_flatbuffers_table_init_root(&table, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));
  nr_cmd_appinfo_process_harvest_timing(&table, &app);
  tlib_pass_if_uint64_t_equal("set timestamp", 1234 * NR_TIME_DIVISOR,
                              app.harvest.connect_timestamp);
  tlib_pass_if_uint64_t_equal("set frequency", 56 * NR_TIME_DIVISOR,
                              app.harvest.frequency);
  nr_flatbuffers_destroy(&fb);

  /*
   * Test : Only frequency set.
   */
  fb = create_app_reply_timing_flatbuffer(0, 56);
  nr_flatbuffers_table_init_root(&table, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));
  nr_cmd_appinfo_process_harvest_timing(&table, &app);
  tlib_fail_if_uint64_t_equal("unset timestamp", 0,
                              app.harvest.connect_timestamp);
  tlib_pass_if_uint64_t_equal("set frequency", 56 * NR_TIME_DIVISOR,
                              app.harvest.frequency);
  nr_flatbuffers_destroy(&fb);

  /*
   * Test : Neither field set.
   */
  fb = create_app_reply_timing_flatbuffer(0, 0);
  nr_flatbuffers_table_init_root(&table, nr_flatbuffers_data(fb),
                                 nr_flatbuffers_len(fb));
  nr_cmd_appinfo_process_harvest_timing(&table, &app);
  tlib_fail_if_uint64_t_equal("unset timestamp", 0,
                              app.harvest.connect_timestamp);
  tlib_pass_if_uint64_t_equal("unset frequency", 60 * NR_TIME_DIVISOR,
                              app.harvest.frequency);
  nr_flatbuffers_destroy(&fb);
}

static void test_process_event_harvest_config(void) {
  nr_app_limits_t app_limits_all_default = default_app_limits();
  nr_app_limits_t app_limits_all_enabled = {
      .analytics_events = 833,
      .custom_events = 833,
      .error_events = 833,
      .span_events = 833,
  };
  nr_app_limits_t app_limits_all_zero = {
      .analytics_events = 0,
      .custom_events = 0,
      .error_events = 0,
      .span_events = 0,
  };
  nr_app_limits_t app_limits;
  nrobj_t* array = nro_new_array();
  nrobj_t* empty = nro_new_hash();
  nrobj_t* limits_disabled = nro_create_from_json(
      "{"
      "\"harvest_limits\":{"
      "\"analytic_event_data\":0,"
      "\"custom_event_data\":0,"
      "\"error_event_data\":0,"
      "\"span_event_data\":0"
      "}"
      "}");
  nrobj_t* limits_enabled = nro_create_from_json(
      "{"
      "\"harvest_limits\":{"
      "\"analytic_event_data\":833,"
      "\"custom_event_data\":833,"
      "\"error_event_data\":833,"
      "\"span_event_data\":833"
      "}"
      "}");

  app_limits = app_limits_all_zero;
  nr_cmd_appinfo_process_event_harvest_config(NULL, &app_limits);
  tlib_pass_if_bytes_equal("a NULL config should enable all event types",
                           &app_limits_all_default, sizeof(nr_app_limits_t),
                           &app_limits, sizeof(nr_app_limits_t));

  app_limits = app_limits_all_zero;
  nr_cmd_appinfo_process_event_harvest_config(array, &app_limits);
  tlib_pass_if_bytes_equal("an invalid config should enable all event types",
                           &app_limits_all_default, sizeof(nr_app_limits_t),
                           &app_limits, sizeof(nr_app_limits_t));

  app_limits = app_limits_all_zero;
  nr_cmd_appinfo_process_event_harvest_config(empty, &app_limits);
  tlib_pass_if_bytes_equal("an empty config should enable all event types",
                           &app_limits_all_default, sizeof(nr_app_limits_t),
                           &app_limits, sizeof(nr_app_limits_t));

  app_limits = app_limits_all_zero;
  nr_cmd_appinfo_process_event_harvest_config(limits_disabled, &app_limits);
  tlib_pass_if_bytes_equal(
      "a config with all types disabled should disable all event types",
      &app_limits_all_zero, sizeof(nr_app_limits_t), &app_limits,
      sizeof(nr_app_limits_t));

  app_limits = app_limits_all_zero;
  nr_cmd_appinfo_process_event_harvest_config(limits_enabled, &app_limits);
  tlib_pass_if_bytes_equal(
      "a config with all types enabled should enable all event types",
      &app_limits_all_enabled, sizeof(nr_app_limits_t), &app_limits,
      sizeof(nr_app_limits_t));

  nro_delete(array);
  nro_delete(empty);
  nro_delete(limits_disabled);
  nro_delete(limits_enabled);
}

static void test_process_get_harvest_limit(void) {
  nrobj_t* array = nro_new_array();
  nrobj_t* limits = nro_create_from_json(
      "{"
      "\"analytic_event_data\":833,"
      "\"custom_event_data\":0,"
      "\"error_event_data\":null,"
      "\"negative_value\":-42,"
      "\"string_value\":\"foo\""
      "}");

  tlib_pass_if_int_equal(
      "NULL limits should return the default value for any key", 100,
      nr_cmd_appinfo_process_get_harvest_limit(NULL, "analytic_event_data",
                                               100));

  tlib_pass_if_int_equal(
      "NULL keys should return the default value", 100,
      nr_cmd_appinfo_process_get_harvest_limit(limits, NULL, 100));

  tlib_pass_if_int_equal("a non-hash object should return the default value",
                         100,
                         nr_cmd_appinfo_process_get_harvest_limit(
                             array, "analytic_event_data", 100));

  tlib_pass_if_int_equal(
      "missing keys should return the default value", 100,
      nr_cmd_appinfo_process_get_harvest_limit(limits, "span_event_data", 100));

  tlib_pass_if_int_equal("null values should return the default value", 100,
                         nr_cmd_appinfo_process_get_harvest_limit(
                             limits, "error_event_data", 100));

  tlib_pass_if_int_equal(
      "non-integer values should return the default value", 100,
      nr_cmd_appinfo_process_get_harvest_limit(limits, "string_value", 100));

  tlib_pass_if_int_equal(
      "non-zero integers should return the actual value", -42,
      nr_cmd_appinfo_process_get_harvest_limit(limits, "negative_value", 100));

  tlib_pass_if_int_equal("non-zero integers should return the actual value",
                         833,
                         nr_cmd_appinfo_process_get_harvest_limit(
                             limits, "analytic_event_data", 100));

  tlib_pass_if_int_equal("zero integers should return zero", 0,
                         nr_cmd_appinfo_process_get_harvest_limit(
                             limits, "custom_event_data", 100));

  nro_delete(array);
  nro_delete(limits);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* vp NRUNUSED) {
  test_create_empty_query();
  test_create_query();

  test_process_null_reply();
  test_process_null_app();
  test_process_unknown_app();
  test_process_invalid_app();
  test_process_disconnected_app();
  test_process_still_valid_app();
  test_process_connected_app_missing_json();
  test_process_connected_app();
  test_process_missing_body();
  test_process_wrong_body_type();
  test_process_lasp_connected_app();
  test_process_harvest_timing_connected_app();
  test_process_harvest_timing();
  test_process_event_harvest_config();
  test_process_get_harvest_limit();
}
