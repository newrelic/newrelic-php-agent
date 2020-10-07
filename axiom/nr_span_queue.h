/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SPAN_QUEUE_HDR
#define NR_SPAN_QUEUE_HDR

#include "nr_span_encoding.h"
#include "nr_span_event.h"
#include "util_time.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * A span queue. Spans pushed into this queue will be held until the queue fills
 * or a timeout is hit, at which point the queue will be flushed to a handler.
 */
typedef struct _nr_span_queue_t nr_span_queue_t;

/*
 * A span queue batch handler. This function receives the encoded span batch,
 * along with whatever userdata is registered.
 *
 * Note that ownership of encoded passes to the handler, and therefore the
 * handler is responsible for invoking nr_span_encoding_result_deinit().
 */
typedef bool (*nr_span_queue_batch_handler_t)(
    nr_span_encoding_result_t* encoded,
    void* userdata);

/*
 * Purpose : Create a new span queue.
 *
 * Params  : 1. The maximum number of spans that may be enqueued before the
 *              queue is flushed.
 *           2. The maximum length of time that may pass, in microseconds,
 *              before the queue is flushed.
 *           3. The handler that will received flushed span batches.
 *           4. Userdata to give to the handler when invoked.
 *
 * Returns : A new span queue, which must be destroyed with
 *           nr_span_queue_destroy(), or NULL on error.
 */
extern nr_span_queue_t* nr_span_queue_create(
    size_t batch_size,
    nrtime_t batch_timeout,
    nr_span_queue_batch_handler_t batch_handler,
    void* batch_handler_userdata);

/*
 * Purpose : Destroy a span queue.
 *
 * Params  : 1. A pointer to the span queue.
 *
 * Warning : This function will not flush spans currently in the queue; call
 *           nr_span_queue_flush() before this function if you don't want to
 *           lose spans.
 */
extern void nr_span_queue_destroy(nr_span_queue_t** queue_ptr);

/*
 * Purpose : Flush a span queue to the handler.
 *
 * Params  : 1. The queue to flush.
 *
 * Returns : True if the flush succeeded; false otherwise.
 */
extern bool nr_span_queue_flush(nr_span_queue_t* queue);

/*
 * Purpose : Push a new span event into the queue.
 *
 * Params  : 1. The span queue.
 *           2. The span event, which will be owned by the span queue
 *              hereafter.
 *
 * Returns : True if the push succeeded; false otherwise.
 */
extern bool nr_span_queue_push(nr_span_queue_t* queue, nr_span_event_t* event);

#endif /* NR_SPAN_QUEUE_HDR */
