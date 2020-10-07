/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_span_encoding.h"
#include "nr_span_event.h"
#include "nr_span_queue.h"
#include "nr_span_queue_private.h"
#include "nr_txn.h"
#include "util_logging.h"

static nr_span_batch_t* nr_span_batch_create(size_t capacity) {
  nr_span_batch_t* batch = nr_malloc(sizeof(nr_span_batch_t)
                                     + capacity * sizeof(nr_span_event_t*));

  batch->capacity = capacity;
  batch->used = 0;
  batch->start_time = nr_get_time();

  return batch;
}

static void nr_span_batch_destroy(nr_span_batch_t** batch_ptr) {
  nr_span_batch_t* batch;
  size_t i;

  if (NULL == batch_ptr || NULL == *batch_ptr) {
    return;
  }

  batch = *batch_ptr;
  for (i = 0; i < batch->used; i++) {
    nr_span_event_destroy(&batch->spans[i]);
  }

  nr_realfree((void**)batch_ptr);
}

nr_span_queue_t* nr_span_queue_create(
    size_t batch_size,
    nrtime_t batch_timeout,
    nr_span_queue_batch_handler_t batch_handler,
    void* batch_handler_userdata) {
  nr_span_queue_t* queue;

  if (0 == batch_size || 0 == batch_timeout || NULL == batch_handler) {
    return NULL;
  }

  queue = nr_malloc(sizeof(nr_span_queue_t));
  queue->batch_size = batch_size;
  queue->batch_timeout = batch_timeout;
  queue->batch_handler = batch_handler;
  queue->batch_handler_userdata = batch_handler_userdata;
  queue->current_batch = nr_span_batch_create(batch_size);

  return queue;
}

void nr_span_queue_destroy(nr_span_queue_t** queue_ptr) {
  if (NULL == queue_ptr || NULL == *queue_ptr) {
    return;
  }

  nr_span_batch_destroy(&(*queue_ptr)->current_batch);
  nr_realfree((void**)queue_ptr);
}

bool nr_span_queue_flush(nr_span_queue_t* queue) {
  nr_span_batch_t* batch;
  nr_span_encoding_result_t encoded = NR_SPAN_ENCODING_RESULT_INIT;
  bool rv = true;

  if (NULL == queue) {
    return false;
  }

  // Even if it's a zero length batch, we should re-create to reset the timer.
  batch = queue->current_batch;
  queue->current_batch = nr_span_batch_create(queue->batch_size);

  // Short circuit if there's nothing to do.
  if (0 == batch->used) {
    goto end;
  }

  nrl_verbosedebug(NRL_AGENT,
                   "flushing a queue of %zu span(s) to the span batch handler",
                   batch->used);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
  if (!nr_span_encoding_batch_v1((const nr_span_event_t**)batch->spans,
                                 batch->used, &encoded)) {
    nrl_warning(NRL_AGENT, "cannot encode span batch with %zu span(s)",
                batch->used);
    rv = false;
    goto end;
  }
#pragma GCC diagnostic pop

  // Since the encoded result won't ever be touched again, this could be moved
  // to a separate thread, either here or in the handler.
  rv = (queue->batch_handler)(&encoded, queue->batch_handler_userdata);

end:
  nr_span_batch_destroy(&batch);
  return rv;
}

bool nr_span_queue_push(nr_span_queue_t* queue, nr_span_event_t* event) {
  if (NULL == queue || NULL == event) {
    nr_span_event_destroy(&event);
    return false;
  }

  if (nrunlikely(NULL == queue->current_batch)) {
    nr_span_event_destroy(&event);
    return false;
  }

  if (queue->current_batch->used >= queue->current_batch->capacity
      || nr_get_time()
             > queue->current_batch->start_time + queue->batch_timeout) {
    // If the flush fails, nr_span_queue_flush() will have logged a message.
    // We'll swallow sadness and move on, since we still want to enqueue the
    // span event we were given.
    nr_span_queue_flush(queue);
  }

  queue->current_batch->spans[queue->current_batch->used++] = event;

  return true;
}
