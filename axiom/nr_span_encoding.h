/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SPAN_ENCODING_HDR
#define NR_SPAN_ENCODING_HDR

#include "nr_axiom.h"
#include "nr_span_event.h"

#include <stddef.h>
#include <stdint.h>

typedef struct _nr_span_encoding_result_t {
  uint8_t* data;
  size_t len;
  size_t span_count;
} nr_span_encoding_result_t;

// A convenience constant to initialise a result.
static const nr_span_encoding_result_t NR_SPAN_ENCODING_RESULT_INIT = {
    .data = NULL,
    .len = 0,
    .span_count = 0,
};

/*
 * Purpose : Encode an array of span events into a v1 8T span batch.
 *
 * Params  : 1. The span events to encode.
 *           2. The number of span events.
 *           3. A pointer to the result structure.
 *
 * Returns : True on success; false otherwise.
 */
extern bool nr_span_encoding_batch_v1(const nr_span_event_t** events,
                                      size_t len,
                                      nr_span_encoding_result_t* result);

/*
 * Purpose : Encode a single span event into a v1 8T span.
 *
 * Params  : 1. The span event to encode.
 *           2. A pointer to the result structure.
 *
 * Returns : True on success; false otherwise.
 */
extern bool nr_span_encoding_single_v1(const nr_span_event_t* event,
                                       nr_span_encoding_result_t* result);

/*
 * Purpose : Free the data contained within a result.
 *
 * Params  : 1. A pointer to the result structure.
 */
extern void nr_span_encoding_result_deinit(nr_span_encoding_result_t* result);

#endif /* NR_SPAN_ENCODING_HDR */
