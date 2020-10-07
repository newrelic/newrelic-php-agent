/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "util_buffer.h"
#include "util_flatbuffers.h"
#include "util_network.h"
#include "util_syscalls.h"

#include "tlib_main.h"
#include "test_app_helpers.h"

#include "mock_agent.c"

static void test_tx(void) {
  const char* agent_run_id = "01234567";
  nrbuf_t* buf;
  const uint8_t* data;
  nr_span_encoding_result_t encoded = NR_SPAN_ENCODING_RESULT_INIT;
  int socks[2];
  nr_span_event_t* span = nr_span_event_create();
  const nr_span_event_t* spans[] = {span};
  nr_flatbuffers_table_t tbl;

  nbsockpair(socks);
  nr_span_encoding_batch_v1(spans, 1, &encoded);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("invalid daemon fd",
                              nr_cmd_span_batch_tx(-1, agent_run_id, &encoded));
  tlib_pass_if_status_failure("NULL agent run ID",
                              nr_cmd_span_batch_tx(socks[0], NULL, &encoded));
  tlib_pass_if_status_failure(
      "NULL span batch", nr_cmd_span_batch_tx(socks[0], agent_run_id, NULL));

  /*
   * Test : Empty batches.
   */
  tlib_pass_if_status_success(
      "zero length batch", nr_cmd_span_batch_tx(socks[0], agent_run_id,
                                                &((nr_span_encoding_result_t){
                                                    .len = 0,
                                                    .span_count = 1,
                                                })));

  tlib_pass_if_status_success(
      "zero span count batch",
      nr_cmd_span_batch_tx(socks[0], agent_run_id,
                           &((nr_span_encoding_result_t){
                               .len = 1,
                               .span_count = 0,
                           })));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_status_success(
      "valid span batch",
      nr_cmd_span_batch_tx(socks[0], agent_run_id, &encoded));

  // Read what was transmitted back and decode it.
  buf = nr_network_receive(socks[1], 100);
  nr_flatbuffers_table_init_root(&tbl, (const uint8_t*)nr_buffer_cptr(buf),
                                 nr_buffer_len(buf));

  tlib_pass_if_int_equal("span batch has the correct message type",
                         MESSAGE_BODY_SPAN_BATCH,
                         nr_flatbuffers_table_read_i8(
                             &tbl, MESSAGE_FIELD_DATA_TYPE, MESSAGE_BODY_NONE));

  tlib_fail_if_int_equal(
      "span batch has a data field", 0,
      nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA));

  tlib_pass_if_uint64_t_equal(
      "span count is correct", 1,
      nr_flatbuffers_table_read_u64(&tbl, SPAN_BATCH_FIELD_COUNT, 0));

  data = (const uint8_t*)nr_flatbuffers_table_read_bytes(
      &tbl, SPAN_BATCH_FIELD_ENCODED);
  tlib_pass_if_bytes_equal("span encoded data is correct", encoded.data,
                           encoded.len, data, encoded.len);

  nr_buffer_destroy(&buf);

  nr_close(socks[0]);
  nr_close(socks[1]);
  nr_span_encoding_result_deinit(&encoded);
  nr_span_event_destroy(&span);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_tx();
}
