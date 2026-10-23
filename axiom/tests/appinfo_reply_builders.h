/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Builders for faux APPINFO replies from the daemon, shared by the tests
 * that need to play the daemon's side of an APPINFO exchange.
 */
#ifndef APPINFO_REPLY_BUILDERS_HDR
#define APPINFO_REPLY_BUILDERS_HDR

#include "nr_axiom.h"

#include "nr_commands_private.h"
#include "util_flatbuffers.h"

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

#endif /* APPINFO_REPLY_BUILDERS_HDR */
