/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <errno.h>

#include "nr_agent.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "util_errno.h"
#include "util_flatbuffers.h"
#include "util_logging.h"
#include "util_network.h"

#define NR_SPAN_BATCH_SEND_TIMEOUT_MSEC 500

static uint32_t nr_span_batch_prepend_batch(
    nr_flatbuffer_t* fb,
    const nr_span_encoding_result_t* encoded_batch) {
  uint32_t offset;

  offset = nr_flatbuffers_prepend_bytes(fb, encoded_batch->data,
                                        encoded_batch->len);

  nr_flatbuffers_object_begin(fb, SPAN_BATCH_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, SPAN_BATCH_FIELD_ENCODED, offset,
                                        0);
  nr_flatbuffers_object_prepend_u64(fb, SPAN_BATCH_FIELD_COUNT,
                                    encoded_batch->span_count, 0);

  return nr_flatbuffers_object_end(fb);
}

static nr_flatbuffer_t* nr_span_batch_encode(
    const char* agent_run_id,
    const nr_span_encoding_result_t* encoded_batch) {
  nr_flatbuffer_t* fb;
  uint32_t message;
  uint32_t agent_run_id_offset;
  uint32_t span_batch;

  fb = nr_flatbuffers_create(0);
  span_batch = nr_span_batch_prepend_batch(fb, encoded_batch);
  agent_run_id_offset = nr_flatbuffers_prepend_string(fb, agent_run_id);

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, span_batch, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_SPAN_BATCH, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id_offset, 0);
  message = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_finish(fb, message);

  return fb;
}

nr_status_t (*nr_cmd_span_batch_hook)(
    int daemon_fd,
    const char* agent_run_id,
    const nr_span_encoding_result_t* encoded_batch)
    = NULL;

nr_status_t nr_cmd_span_batch_tx(
    int daemon_fd,
    const char* agent_run_id,
    const nr_span_encoding_result_t* encoded_batch) {
  nr_flatbuffer_t* msg;
  size_t msglen;
  nr_status_t st;

  if (nr_cmd_span_batch_hook) {
    return nr_cmd_span_batch_hook(daemon_fd, agent_run_id, encoded_batch);
  }

  if (daemon_fd < 0 || NULL == agent_run_id || NULL == encoded_batch) {
    return NR_FAILURE;
  }

  if (0 == encoded_batch->len || 0 == encoded_batch->span_count) {
    return NR_SUCCESS;
  }

  msg = nr_span_batch_encode(agent_run_id, encoded_batch);
  msglen = nr_flatbuffers_len(msg);

  nrl_verbosedebug(NRL_DAEMON, "sending span batch message, len=%zu", msglen);

  if (nr_command_is_flatbuffer_invalid(msg, msglen)) {
    nr_flatbuffers_destroy(&msg);
    return NR_FAILURE;
  }

  nr_agent_lock_daemon_mutex();
  {
    nrtime_t deadline;

    deadline = nr_get_time()
               + (NR_SPAN_BATCH_SEND_TIMEOUT_MSEC * NR_TIME_DIVISOR_MS);
    st = nr_write_message(daemon_fd, nr_flatbuffers_data(msg), msglen,
                          deadline);
  }
  nr_agent_unlock_daemon_mutex();
  nr_flatbuffers_destroy(&msg);

  if (NR_SUCCESS != st) {
    nrl_error(NRL_DAEMON, "SPAN_BATCH failure: len=%zu errno=%s", msglen,
              nr_errno(errno));
    nr_agent_close_daemon_connection();
  }

  return st;
}
