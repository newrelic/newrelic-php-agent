/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_span_encoding.h"

#include "nr_span_encoding_private.h"
#include "nr_span_event_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "v1.pb-c.h"

static inline void pack_batch(const Com__Newrelic__Trace__V1__SpanBatch* batch,
                              nr_span_encoding_result_t* result) {
  result->span_count = batch->n_spans;
  result->len = com__newrelic__trace__v1__span_batch__get_packed_size(batch);
  result->data = nr_malloc(result->len);
  com__newrelic__trace__v1__span_batch__pack(batch, result->data);
}

bool nr_span_encoding_batch_v1(const nr_span_event_t** events,
                               size_t len,
                               nr_span_encoding_result_t* result) {
  Com__Newrelic__Trace__V1__SpanBatch batch;
  nr_span_encoding_context_t ctx;
  size_t i;
  bool rv = false;

  if (NULL == events || NULL == result) {
    return false;
  }

  com__newrelic__trace__v1__span_batch__init(&batch);

  if (0 == len) {
    batch.n_spans = 0;
    batch.spans = NULL;

    pack_batch(&batch, result);
    return true;
  }

  nr_span_encoding_context_init(&ctx, len);
  batch.n_spans = len;
  batch.spans = nr_calloc(len, sizeof(Com__Newrelic__Trace__V1__Span*));

  for (i = 0; i < len; i++) {
    batch.spans[i] = nr_slab_next(ctx.span_slab);

    if (!nr_span_encoding_encode_span_v1(events[i], batch.spans[i], &ctx)) {
      nrl_warning(NRL_AGENT, "%s: error encoding span event %zu", __func__, i);
      goto end;
    }
  }

  pack_batch(&batch, result);
  rv = true;

end:
  nr_free(batch.spans);
  nr_span_encoding_context_deinit(&ctx);

  return rv;
}

bool nr_span_encoding_single_v1(const nr_span_event_t* event,
                                nr_span_encoding_result_t* result) {
  nr_span_encoding_context_t ctx;
  bool rv;
  Com__Newrelic__Trace__V1__Span span;

  if (NULL == event || NULL == result) {
    return false;
  }

  // Technically, we're doing a bit more work than we need to here: we don't
  // need a span slab to allocate one span! Nevertheless, we'll reuse the same
  // context here for simplicity; this function is only used for testing right
  // now anyway.
  nr_span_encoding_context_init(&ctx, 1);

  rv = nr_span_encoding_encode_span_v1(event, &span, &ctx);
  if (!rv) {
    goto end;
  }

  result->span_count = 1;
  result->len = com__newrelic__trace__v1__span__get_packed_size(&span);
  result->data = nr_malloc(result->len);
  com__newrelic__trace__v1__span__pack(&span, result->data);

end:
  nr_span_encoding_context_deinit(&ctx);
  return rv;
}

void nr_span_encoding_result_deinit(nr_span_encoding_result_t* result) {
  if (result) {
    nr_free(result->data);
  }
}

bool nr_span_encoding_encode_attribute_value_v1(
    const nrobj_t* obj,
    Com__Newrelic__Trace__V1__AttributeValue* value) {
  com__newrelic__trace__v1__attribute_value__init(value);

  switch (nro_type(obj)) {
    case NR_OBJECT_INT:
    case NR_OBJECT_LONG:
      value->value_case
          = COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_INT_VALUE;
      value->int_value = (int64_t)nro_get_long(obj, NULL);
      break;

    case NR_OBJECT_ULONG:
      value->value_case
          = COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_INT_VALUE;
      value->int_value = (int64_t)nro_get_ulong(obj, NULL);
      break;

    case NR_OBJECT_DOUBLE:
      value->value_case
          = COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_DOUBLE_VALUE;
      value->double_value = nro_get_double(obj, NULL);
      break;

    case NR_OBJECT_BOOLEAN:
      value->value_case
          = COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_BOOL_VALUE;
      value->bool_value = nro_get_boolean(obj, NULL);
      break;

    case NR_OBJECT_STRING:
      value->value_case
          = COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE_STRING_VALUE;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
      value->string_value = (char*)nro_get_string(obj, NULL);
#pragma GCC diagnostic pop
      break;

    case NR_OBJECT_INVALID:
    case NR_OBJECT_HASH:
    case NR_OBJECT_ARRAY:
    case NR_OBJECT_NONE:
    case NR_OBJECT_JSTRING:
    default:
      value->value_case
          = COM__NEWRELIC__TRACE__V1__ATTRIBUTE_VALUE__VALUE__NOT_SET;
      return false;
  }

  return true;
}

// This next bit is hideous, but it gets us type safety across the disjoint
// entry types.
//
// To recap: the protoc-c compiler has kindly generated us three *Entry types to
// represent attribute maps. These types are all identical. However, because C
// doesn't support structural typing, we can't just write one implementation of
// a function to encode an nrobj_t hash to an array of entries.
//
// (Well, we _can_, but that involves a bunch of scary assumptions that can
// never change about the generated code and a lot of void * pointers, and I'm
// trying to kick my void * habit.)
//
// So we'll define this GENERATE_SERIALISE_FUNC() macro that templates our
// functions to take an nrobj_t and fill in an Entry array for use in later
// encoding endeavours. If you use Fira Code, you get to see the *** ligature
// because there is an honest-to-God triple pointer in here. (Technically, it's
// an output parameter for a double pointer array, but I'm not sure that makes
// it better.)

// Where we're going, we don't need cast qualifier warnings.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#define GENERATE_SERIALISE_FUNC(NAME, TYPE, INIT_FUNC)                       \
  static bool NAME(const nrobj_t* obj, TYPE*** out_ptr, size_t* out_len,     \
                   nr_span_encoding_context_t* ctx) {                        \
    int len = nro_getsize(obj);                                              \
                                                                             \
    *out_len = (size_t)len;                                                  \
                                                                             \
    if (len > 0) {                                                           \
      int i;                                                                 \
                                                                             \
      *out_ptr = nr_calloc(len, sizeof(TYPE*));                              \
      nr_vector_push_back(&ctx->proto_arrays, *out_ptr);                     \
                                                                             \
      for (i = 0; i < len; i++) {                                            \
        Com__Newrelic__Trace__V1__AttributeValue* av                         \
            = nr_slab_next(ctx->attribute_value_slab);                       \
        TYPE* entry = nr_slab_next(ctx->entry_slab);                         \
        const char* key = NULL;                                              \
        const nrobj_t* value;                                                \
                                                                             \
        /* Hashes use 1-based indexing, like arrays. */                      \
        value = nro_get_hash_value_by_index(obj, i + 1, NULL, &key);         \
        if (NULL == value) {                                                 \
          /* Yikes. We really shouldn't get here. Something is spectacularly \
           * wrong, so let's just bail out. */                               \
          return false;                                                      \
        }                                                                    \
                                                                             \
        nr_span_encoding_encode_attribute_value_v1(value, av);               \
                                                                             \
        INIT_FUNC(entry);                                                    \
        entry->key = (char*)key;                                             \
        entry->value = av;                                                   \
        (*out_ptr)[i] = entry;                                               \
      }                                                                      \
    } else {                                                                 \
      *out_ptr = NULL;                                                       \
    }                                                                        \
                                                                             \
    return true;                                                             \
  }

// All right, you can stop averting your eyes, because it's time to define some
// functions.

GENERATE_SERIALISE_FUNC(nr_span_encoding_intrinsics_to_infinite_v1,
                        Com__Newrelic__Trace__V1__Span__IntrinsicsEntry,
                        com__newrelic__trace__v1__span__intrinsics_entry__init)

GENERATE_SERIALISE_FUNC(
    nr_span_encoding_agent_attributes_to_infinite_v1,
    Com__Newrelic__Trace__V1__Span__AgentAttributesEntry,
    com__newrelic__trace__v1__span__agent_attributes_entry__init)

GENERATE_SERIALISE_FUNC(
    nr_span_encoding_user_attributes_to_infinite_v1,
    Com__Newrelic__Trace__V1__Span__UserAttributesEntry,
    com__newrelic__trace__v1__span__user_attributes_entry__init)
#pragma GCC diagnostic pop

bool nr_span_encoding_encode_span_v1(const nr_span_event_t* event,
                                     Com__Newrelic__Trace__V1__Span* span,
                                     nr_span_encoding_context_t* ctx) {
  if (nrunlikely(NULL == event || NULL == span || NULL == ctx)) {
    return false;
  }

  com__newrelic__trace__v1__span__init(span);
  span->trace_id = event->trace_id;

  if (!nr_span_encoding_intrinsics_to_infinite_v1(
          event->intrinsics, &span->intrinsics, &span->n_intrinsics, ctx)) {
    nrl_warning(NRL_AGENT,
                "error encoding span event intrinsics; dropping span event");
    return false;
  }

  if (!nr_span_encoding_agent_attributes_to_infinite_v1(
          event->agent_attributes, &span->agent_attributes,
          &span->n_agent_attributes, ctx)) {
    nrl_warning(
        NRL_AGENT,
        "error encoding span event agent attributes; dropping span event");
    return false;
  }

  if (!nr_span_encoding_user_attributes_to_infinite_v1(
          event->user_attributes, &span->user_attributes,
          &span->n_user_attributes, ctx)) {
    nrl_warning(
        NRL_AGENT,
        "error encoding span event user attributes; dropping span event");
    return false;
  }

  return true;
}

static void nr_span_encoding_proto_array_destroy(void* ptr,
                                                 void* userdata NRUNUSED) {
  nr_free(ptr);
}

void nr_span_encoding_context_init(nr_span_encoding_context_t* ctx,
                                   size_t span_count) {
  size_t entry_size = 0;
  static const size_t entry_sizes[3] = {
      sizeof(Com__Newrelic__Trace__V1__Span__AgentAttributesEntry),
      sizeof(Com__Newrelic__Trace__V1__Span__IntrinsicsEntry),
      sizeof(Com__Newrelic__Trace__V1__Span__UserAttributesEntry),
  };
  size_t i;

  // We need to know what the largest of the attribute entry types are. In
  // practice, they should all be the same, but this is a simple enough check
  // that we might as well do it and be extra safe.
  for (i = 0; i < (sizeof(entry_sizes) / sizeof(entry_sizes[0])); i++) {
    if (entry_sizes[i] > entry_size) {
      entry_size = entry_sizes[i];
    }
  }

  // We don't know how many attributes there will be, but we can at least make
  // the page size reasonably large to start with.
  ctx->attribute_value_slab = nr_slab_create(
      sizeof(Com__Newrelic__Trace__V1__AttributeValue),
      span_count * sizeof(Com__Newrelic__Trace__V1__AttributeValue));

  // Each span event has three sets of "entries" (attributes), so we can size
  // the slab to the size we need right away and it should never need to grow.
  ctx->entry_slab = nr_slab_create(entry_size, 3 * span_count * entry_size);

  // Again, since we know the number of span events, we can size this slab in
  // advance.
  ctx->span_slab
      = nr_slab_create(sizeof(Com__Newrelic__Trace__V1__Span),
                       span_count * sizeof(Com__Newrelic__Trace__V1__Span));

  // Finally, we'll initialise the vector we're going to use as scratch space
  // for the double pointer arrays that are needed to get the entries onto the
  // encoded spans.
  nr_vector_init(&ctx->proto_arrays, 3 * span_count,
                 nr_span_encoding_proto_array_destroy, NULL);
}

void nr_span_encoding_context_deinit(nr_span_encoding_context_t* ctx) {
  nr_slab_destroy(&ctx->attribute_value_slab);
  nr_slab_destroy(&ctx->entry_slab);
  nr_slab_destroy(&ctx->span_slab);
  nr_vector_deinit(&ctx->proto_arrays);
}
