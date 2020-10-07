/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_span_encoding.h"
#include "nr_span_event.h"
#include "nr_span_queue.h"
#include "nr_span_queue_private.h"
#include "util_sleep.h"

#include "tlib_main.h"

static void handle_result(nr_span_encoding_result_t* result, uint64_t* count) {
  tlib_pass_if_not_null("result must be valid", result);
  nr_span_encoding_result_deinit(result);

  if (count) {
    *count += 1;
  }
}

static bool failure_handler(nr_span_encoding_result_t* result, void* count) {
  handle_result(result, (uint64_t*)count);
  return false;
}

static bool success_handler(nr_span_encoding_result_t* result, void* count) {
  handle_result(result, (uint64_t*)count);
  return true;
}

static void test_create_destroy(void) {
  uint64_t flush_count = 0;
  nr_span_queue_t* queue = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null(
      "0 batch size",
      nr_span_queue_create(0, 1 * NR_TIME_DIVISOR_MS, success_handler, NULL));

  tlib_pass_if_null("0 batch timeout",
                    nr_span_queue_create(100, 0, success_handler, NULL));

  tlib_pass_if_null(
      "NULL batch handler",
      nr_span_queue_create(100, 1 * NR_TIME_DIVISOR_MS, NULL, NULL));

  nr_span_queue_destroy(NULL);
  nr_span_queue_destroy(&queue);

  /*
   * Test : Normal operation.
   */
  queue = nr_span_queue_create(100, 1 * NR_TIME_DIVISOR_MS, success_handler,
                               &flush_count);
  tlib_pass_if_not_null("valid queue", queue);
  nr_span_queue_destroy(&queue);
  tlib_pass_if_uint64_t_equal("destroying a queue does not automatically flush",
                              0, flush_count);
}

static void test_flush(void) {
  uint64_t flush_count;
  nr_span_queue_t* queue;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("NULL queue", false, nr_span_queue_flush(NULL));

  /*
   * Test : Empty flush.
   */
  flush_count = 0;
  queue = nr_span_queue_create(100, 1 * NR_TIME_DIVISOR_MS, success_handler,
                               &flush_count);
  tlib_pass_if_bool_equal("empty flush", true, nr_span_queue_flush(queue));
  tlib_pass_if_uint64_t_equal("empty flushes should not call the handler", 0,
                              flush_count);
  nr_span_queue_destroy(&queue);

  /*
   * Test : Successful flush.
   */
  flush_count = 0;
  queue = nr_span_queue_create(100, 1 * NR_TIME_DIVISOR_MS, success_handler,
                               &flush_count);
  nr_span_queue_push(queue, nr_span_event_create());
  tlib_pass_if_bool_equal("successful flush", true, nr_span_queue_flush(queue));
  tlib_pass_if_uint64_t_equal(
      "successful flushes should invoke the handler once", 1, flush_count);
  nr_span_queue_destroy(&queue);

  /*
   * Test : Failed flush.
   */
  flush_count = 0;
  queue = nr_span_queue_create(100, 1 * NR_TIME_DIVISOR_MS, failure_handler,
                               &flush_count);
  nr_span_queue_push(queue, nr_span_event_create());
  tlib_pass_if_bool_equal("failed flush", false, nr_span_queue_flush(queue));
  tlib_pass_if_uint64_t_equal("failed flushes should invoke the handler once",
                              1, flush_count);
  nr_span_queue_destroy(&queue);
}

static void test_push(void) {
  uint64_t flush_count = 0;
  size_t i;
  nr_span_queue_t* queue = nr_span_queue_create(10, 1 * NR_TIME_DIVISOR_MS,
                                                failure_handler, &flush_count);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_bool_equal("NULL queue", false,
                          nr_span_queue_push(NULL, nr_span_event_create()));
  tlib_pass_if_bool_equal("NULL event", false, nr_span_queue_push(queue, NULL));

  /*
   * Test : Batch capacity hit.
   */
  for (i = 0; i < 11; i++) {
    tlib_pass_if_bool_equal(
        "successful push returns true, even if the handler doesn't", true,
        nr_span_queue_push(queue, nr_span_event_create()));
  }
  tlib_pass_if_uint64_t_equal("queue should have been flushed once", 1,
                              flush_count);

  /*
   * Test : Timeout hit.
   */
  flush_count = 0;
  nr_msleep(2);
  tlib_pass_if_bool_equal("another push after the timeout should return true",
                          true,
                          nr_span_queue_push(queue, nr_span_event_create()));
  tlib_pass_if_uint64_t_equal(
      "queue should have been flushed again by the timeout", 1, flush_count);

  nr_span_queue_destroy(&queue);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create_destroy();
  test_flush();
  test_push();
}
