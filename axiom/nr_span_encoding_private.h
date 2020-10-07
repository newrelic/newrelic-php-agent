/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SPAN_ENCODING_PRIVATE_HDR
#define NR_SPAN_ENCODING_PRIVATE_HDR

#include "nr_span_encoding.h"
#include "util_object.h"
#include "util_slab.h"
#include "util_vector.h"
#include "v1.pb-c.h"

// Building a span batch is tricky: the code generated by protobuf-c represents
// maps and repeated fields as C arrays to pointers, but doesn't provide any
// tools to help manage the ownership of the underlying data. As a result, we
// need to be able to allocate attribute values, entries, and spans in a place
// that we control, keep them in memory until encoding is complete, then we can
// dispose of them completely.
//
// This context is used while encoding a span batch to manage that memory. Since
// each of those data types are homogeneous, we can use slabs to allocate those
// instances, and then clean up quickly once we're done.
//
// We also have variable length arrays encapsulating the attribute maps: these
// are owned by the proto_arrays vector, which again simplifies our memory
// management story.
typedef struct _nr_span_encoding_context_t {
  nr_slab_t* attribute_value_slab;
  nr_slab_t* entry_slab;
  nr_slab_t* span_slab;
  nr_vector_t proto_arrays;
} nr_span_encoding_context_t;

/*
 * Purpose : Encode a scalar nrobj_t value into an appropriately typed Protobuf
 *           value.
 *
 * Params  : 1. The value to encode.
 *           2. The Protobuf value to output to.
 *
 * Returns : True if the value can be encoded; false otherwise.
 *
 * Warning : No NULL checks are performed on the parameters.
 */
extern bool nr_span_encoding_encode_attribute_value_v1(
    const nrobj_t* obj,
    Com__Newrelic__Trace__V1__AttributeValue* value);

/*
 * Purpose : Encode a span event into the given Protobuf span struct.
 *
 * Params  : 1. The span event to encode.
 *           2. The Protobuf value to output to.
 *           3. The span encoding context to use for allocations.
 *
 * Returns : True if the span event is encoded; false otherwise.
 *
 * Warning : No NULL checks are performed on the parameters.
 */
extern bool nr_span_encoding_encode_span_v1(
    const nr_span_event_t* event,
    Com__Newrelic__Trace__V1__Span* span,
    nr_span_encoding_context_t* ctx);

/*
 * Purpose : Initialise a span encoding context.
 *
 * Params  : 1. The context to initialise.
 *           2. The number of spans that will be encoded.
 *
 * Warning : No NULL checks are performed on the parameters.
 */
extern void nr_span_encoding_context_init(nr_span_encoding_context_t* ctx,
                                          size_t span_count);

/*
 * Purpose : Finalise a span encoding context.
 *
 * Params  : 1. The context to finalise.
 *
 * Warning : No NULL checks are performed on the parameters.
 */
extern void nr_span_encoding_context_deinit(nr_span_encoding_context_t* ctx);

#endif /* NR_SPAN_ENCODING_PRIVATE_HDR */
