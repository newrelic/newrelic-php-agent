/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SPAN_QUEUE_PRIVATE_HDR
#define NR_SPAN_QUEUE_PRIVATE_HDR

#include "nr_span_queue.h"

typedef struct _nr_span_batch_t {
  size_t capacity;
  size_t used;
  nrtime_t start_time;
  nr_span_event_t* spans[0];
} nr_span_batch_t;

struct _nr_span_queue_t {
  size_t batch_size;
  nrtime_t batch_timeout;
  nr_span_queue_batch_handler_t batch_handler;
  void* batch_handler_userdata;
  nr_span_batch_t* current_batch;
};

#endif /* NR_SPAN_QUEUE_PRIVATE_HDR */
