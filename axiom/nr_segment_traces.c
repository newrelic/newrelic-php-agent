/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_guid.h"
#include "nr_limits.h"
#include "nr_segment.h"
#include "nr_segment_private.h"
#include "nr_segment_traces.h"
#include "nr_segment_tree.h"
#include "nr_txn.h"
#include "util_logging.h"
#include "util_minmax_heap.h"
#include "util_strings.h"

#include <stdio.h>

static void nr_vector_span_event_dtor(void* element, void* userdata NRUNUSED) {
  nr_span_event_destroy((nr_span_event_t**)&element);
}

/*
 * Purpose: Add a key-value pair to a hash in the buffer.
 *
 * If the key-value pair is not the first pair in the hash, it is
 * prepended with a comma.
 *
 * If raw_json is true, the value is added as escaped JSON. Otherwise
 * the value is added to the JSON output as is.
 */
static void add_hash_key_value_to_buffer(nrbuf_t* buf,
                                         const char* key,
                                         const char* value,
                                         bool raw_json) {
  if (NULL == key || NULL == value) {
    return;
  }

  if ('{' != nr_buffer_peek_end(buf)) {
    nr_buffer_add(buf, NR_PSTR(","));
  }

  nr_buffer_add(buf, NR_PSTR("\""));
  nr_buffer_add(buf, key, nr_strlen(key));
  nr_buffer_add(buf, NR_PSTR("\":"));
  if (raw_json) {
    nr_buffer_add(buf, value, nr_strlen(value));
  } else {
    nr_buffer_add_escape_json(buf, value);
  }
}

static void add_hash_key_value_to_buffer_int(nrbuf_t* buf,
                                             const char* key,
                                             const uint64_t* value) {
  if (NULL == key || NULL == value) {
    return;
  }

  if ('{' != nr_buffer_peek_end(buf)) {
    nr_buffer_add(buf, NR_PSTR(","));
  }

  nr_buffer_add(buf, NR_PSTR("\""));
  nr_buffer_add(buf, key, nr_strlen(key));
  nr_buffer_add(buf, NR_PSTR("\":"));
  nr_buffer_write_uint64_t_as_text(buf, *value);
}

/*
 * Purpose: Add the "async_context" attribute to a hash in the buffer.
 *
 * A previous version of this function also used the string table for
 * "async_context", but it turns out that RPM doesn't interpolate keys.
 */
static void add_async_attribute_to_buffer(nrbuf_t* buf,
                                          nr_segment_t* segment,
                                          nrpool_t* segment_names) {
  const char* context;
  uint64_t context_idx;
  char context_idx_str[21] = {0};

  context = nr_string_get(segment->txn->trace_strings, segment->async_context);
  context_idx = nr_string_add(segment_names, context ? context : "<unknown>");

  /* The internal string tables index at 1, and we wish to index by 0 here. */
  context_idx--;

  snprintf(context_idx_str, sizeof(context_idx_str), "`" NR_UINT64_FMT,
           context_idx);

  add_hash_key_value_to_buffer(buf, "async_context", context_idx_str, false);
}

/*
 * Purpose: Add a hash to a hash in the buffer.
 *
 * The hash is added without the leading and trailing '{' and '}'
 * characters.
 *
 * If the hash in the buffer already contains key-value pairs, a comma
 * is added before adding further values.
 */
static void add_attribute_hash_to_buffer(nrbuf_t* buf, nrobj_t* attributes) {
  if (NULL != attributes) {
    char* json = nro_to_json(attributes);
    int json_len = nr_strlen(json);

    /*
     * An empty hash will be a two character string: "{}". If it's longer than
     * that, then there's data, which we should add to the buffer without the
     * surrounding braces.
     */
    if (json_len > 2) {
      if ('{' != nr_buffer_peek_end(buf)) {
        nr_buffer_add(buf, ",", 1);
      }
      nr_buffer_add(buf, json + 1, nr_strlen(json) - 2);
    }

    nr_free(json);
  }
}

/*
 * Purpose: Add typed attributes from a segment to a hash in the buffer.
 */
static void add_typed_attributes_to_buffer(nrbuf_t* buf,
                                           const nr_segment_t* segment) {
  if (NULL == segment->typed_attributes) {
    return;
  }

  switch (segment->type) {
    case NR_SEGMENT_DATASTORE: {
      const nr_segment_datastore_t* data
          = &segment->typed_attributes->datastore;
      add_hash_key_value_to_buffer(buf, "host", data->instance.host, false);
      add_hash_key_value_to_buffer(buf, "database_name",
                                   data->instance.database_name, false);
      add_hash_key_value_to_buffer(buf, "port_path_or_id",
                                   data->instance.port_path_or_id, false);
      add_hash_key_value_to_buffer(buf, "backtrace", data->backtrace_json,
                                   true);
      add_hash_key_value_to_buffer(buf, "explain_plan", data->explain_plan_json,
                                   true);
      add_hash_key_value_to_buffer(buf, "sql", data->sql, false);
      add_hash_key_value_to_buffer(buf, "sql_obfuscated", data->sql_obfuscated,
                                   false);
      add_hash_key_value_to_buffer(buf, "input_query", data->input_query_json,
                                   true);
    } break;
    case NR_SEGMENT_EXTERNAL: {
      const nr_segment_external_t* ext = &segment->typed_attributes->external;
      add_hash_key_value_to_buffer(buf, "uri", ext->uri, false);
      add_hash_key_value_to_buffer(buf, "library", ext->library, false);
      add_hash_key_value_to_buffer(buf, "procedure", ext->procedure, false);
      add_hash_key_value_to_buffer(buf, "transaction_guid",
                                   ext->transaction_guid, false);
      add_hash_key_value_to_buffer_int(buf, "status", &ext->status);
    } break;
    case NR_SEGMENT_MESSAGE: {
      const nr_segment_message_t* message = &segment->typed_attributes->message;
      add_hash_key_value_to_buffer(buf, "destination_name",
                                   message->destination_name, false);
      add_hash_key_value_to_buffer(buf, "messaging_system",
                                   message->messaging_system, false);
      add_hash_key_value_to_buffer(buf, "cloud_region", message->cloud_region,
                                   false);
      add_hash_key_value_to_buffer(buf, "cloud_account_id",
                                   message->cloud_account_id, false);
      add_hash_key_value_to_buffer(buf, "cloud_resource_id",
                                   message->cloud_resource_id, false);
      add_hash_key_value_to_buffer(buf, "server_address",
                                   message->server_address, false);
    } break;
    case NR_SEGMENT_CUSTOM:
    default:
      break;
  }
}

static inline bool nr_segment_is_sampled(const nr_segment_t* segment,
                                         nr_set_t* set) {
  if (nrunlikely(NULL == segment)) {
    return false;
  }

  if (NULL == set) {
    return true;
  }
  return nr_set_contains(set, (const void*)segment);
}

static void nr_segment_traces_stot_iterator_post_callback(
    nr_segment_t* segment,
    nr_segment_userdata_t* userdata) {
  nr_segment_t* current_trace_segment;

  if (nrunlikely(NULL == segment || NULL == userdata)) {
    nrl_warning(
        NRL_SEGMENT,
        "Invalid inputs to the segment iterator: segment=%p; userdata=%p",
        segment, userdata);
    userdata->success = false;
    return;
  }

  current_trace_segment = nr_vector_get(userdata->trace.current_path, 0);

  /*
   * The segment is sampled for the the trace output. It has to be popped off
   * the current trace ancestor path and brackets have to be added to the
   * trace output.
   */
  if (NULL != userdata->trace.buf && segment == current_trace_segment) {
    nr_buffer_add(userdata->trace.buf, "]", 1);
    nr_buffer_add(userdata->trace.buf, "]", 1);

    nr_vector_remove(userdata->trace.current_path, 0,
                     (void**)&current_trace_segment);
  }

  /*
   * If the segment is sampled for the span output, then we need to remove it
   * from the stack of parent IDs.
   */
  if (nr_segment_is_sampled(segment, userdata->spans.sample)) {
    nr_stack_pop(&userdata->spans.parent_ids);
  }
}

static void nr_segment_iteration_pass_trace(nr_segment_t* segment,
                                            nr_segment_userdata_t* userdata,
                                            const char* segment_name) {
  nr_segment_userdata_trace_t* tracedata = &(userdata->trace);
  bool trace_is_sampled = (NULL != tracedata->sample);
  bool segment_is_sampled = nr_set_contains(tracedata->sample, (void*)segment);
  nrpool_t* segment_names = userdata->segment_names;
  nrbuf_t* buf = userdata->trace.buf;
  int idx;
  nr_segment_t* parent = NULL;
  nrobj_t* user_attributes = NULL;
  nrobj_t* agent_attributes = NULL;

  uint64_t start_ms;
  uint64_t stop_ms;

  if (trace_is_sampled && !segment_is_sampled) {
    return;
  }

  /* Get nearest sampled ancestor segment. This will serve as parent. */
  parent = (nr_segment_t*)nr_vector_get(tracedata->current_path, 0);

  /* Update the current ancestor path of segments added to the trace
   * output. */
  nr_vector_push_front(tracedata->current_path, (void*)segment);

  /* Examine the sampled_ancestors.  If the closest sampled ancestor of this
   * segment appears in the set, this means the current segment has a previous
   * sibling. and so the JSON needs a comma. */
  if (nr_set_contains(tracedata->sampled_ancestors_with_child, parent)) {
    nr_buffer_add(buf, ",", 1);
  }

  if (NULL != parent) {
    nr_set_insert(tracedata->sampled_ancestors_with_child, (void*)parent);
  }

  /* Get the name index.
   * The internal string tables index at 1, and we wish to index by 0 here. */
  idx = nr_string_add(segment_names, segment_name);
  idx--;

  /* Every segment's start and stop time are unsigned values, recorded in
   * microseconds relative to the start of the transaction. Convert these
   * values to milliseconds and adjust the stop time if it is recorded as
   * taking place before the start. */
  start_ms = segment->start_time / NR_TIME_DIVISOR_MS;
  stop_ms = segment->stop_time / NR_TIME_DIVISOR_MS;

  if (start_ms > stop_ms) {
    stop_ms = start_ms;
  }

  nr_buffer_add(buf, "[", 1);
  nr_buffer_write_uint64_t_as_text(buf, start_ms);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_write_uint64_t_as_text(buf, stop_ms);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "\"", 1);
  nr_buffer_add(buf, "`", 1);
  nr_buffer_write_uint64_t_as_text(buf, (uint64_t)idx);
  nr_buffer_add(buf, "\"", 1);
  nr_buffer_add(buf, ",", 1);

  /*
   * Segment parameters.
   */
  nr_buffer_add(buf, "{", 1);

  add_typed_attributes_to_buffer(buf, segment);

  if (segment->async_context) {
    add_async_attribute_to_buffer(buf, segment, segment_names);
  }

  if (segment->attributes) {
    user_attributes = nr_attributes_user_to_obj(
        segment->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);
    add_attribute_hash_to_buffer(buf, user_attributes);
    nro_delete(user_attributes);
    /*
     *  Add segment attributes to transaction trace.
     */
    agent_attributes = nr_attributes_agent_to_obj(
        segment->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);
    add_attribute_hash_to_buffer(buf, agent_attributes);
    nro_delete(agent_attributes);
  }

  nr_buffer_add(buf, "}", 1);

  /* And now for all its children. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);
}

static void nr_segment_iteration_pass_span(nr_segment_t* segment,
                                           nr_segment_userdata_t* userdata,
                                           const char* segment_name) {
  nr_segment_userdata_spans_t* spandata = &userdata->spans;
  nr_span_event_t* span;

  if (!nr_segment_is_sampled(segment, spandata->sample)) {
    return;
  }

  span = nr_segment_to_span_event(segment);
  if (span) {
    nr_span_event_set_name(span, segment_name);

    // If this segment is the root segment, then we have to handle things
    // slightly differently: we have to set the entry point attribute, and add
    // any relevant DT attributes.
    if (userdata->txn->segment_root == segment) {
      const nr_distributed_trace_t* dt = userdata->txn->distributed_trace;

      nr_span_event_set_entry_point(span, true);
      nr_span_event_set_parent_id(span,
                                  nr_distributed_trace_inbound_get_guid(dt));
      nr_span_event_set_tracing_vendors(
          span, nr_distributed_trace_inbound_get_tracing_vendors(dt));
      nr_span_event_set_trusted_parent_id(
          span, nr_distributed_trace_inbound_get_trusted_parent_id(dt));
    } else {
      nr_span_event_set_parent_id(
          span, (const char*)nr_stack_get_top(&spandata->parent_ids));
    }

    nr_vector_push_back(spandata->events, span);
  }

  // We have to add the GUID to the span path regardless of whether the span
  // event conversion above succeeded or failed, since the post-callback will
  // pop it from the stack.
  nr_stack_push(&spandata->parent_ids, (void*)segment->id);
}

nr_segment_iter_return_t nr_segment_traces_stot_iterator_callback(
    nr_segment_t* segment,
    void* data) {
  nr_segment_userdata_t* userdata = (nr_segment_userdata_t*)data;
  const nrtxn_t* txn = userdata->txn;
  const char* segment_name;

  segment_name = nr_string_get(txn->trace_strings, segment->name);
  if (NULL == segment_name) {
    segment_name = "<unknown>";
  }

  /*
   * For VERY fast segments, the start time can equal the stop time.
   * Such segments are skipped, as zero duration segments don't make
   * sense.
   */
  if (segment->start_time == segment->stop_time) {
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  /*
   * Sanity check, the segment should have started before it stopped.
   */
  if (segment->start_time > segment->stop_time) {
    nrl_warning(NRL_SEGMENT,
                "Invalid segment '%s': start time (" NR_TIME_FMT
                ") after stop time (" NR_TIME_FMT ")",
                segment_name, segment->start_time, segment->stop_time);
    userdata->success = false;
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  /*
   * Spans are only created if the span event output vector is given.
   */
  if (NULL != userdata->spans.events) {
    nr_segment_iteration_pass_span(segment, userdata, segment_name);
  }

  /*
   * Traces are only created if the trace output buffer is given.
   */
  if (NULL != userdata->trace.buf) {
    nr_segment_iteration_pass_trace(segment, userdata, segment_name);
  }

  /*
   * Register a post-callback to close brackets and to adapt parent stacks.
   */
  return ((nr_segment_iter_return_t){
      .post_callback
      = (nr_segment_post_iter_t)nr_segment_traces_stot_iterator_post_callback,
      .userdata = userdata});
}

bool nr_segment_traces_json_print_segments(nrbuf_t* buf,
                                           nr_vector_t* span_events,
                                           nr_set_t* trace_set,
                                           nr_set_t* span_set,
                                           const nrtxn_t* txn,
                                           nr_segment_t* root,
                                           nrpool_t* segment_names) {
  nr_segment_userdata_t* userdata;

  if (NULL == buf && NULL == span_events) {
    /* The trace output buffer and the span output vector are not given.
     * In that case, there's no work to do.
     */
    return false;
  }

  if ((NULL == txn) || (NULL == segment_names) || (NULL == root)) {
    return false;
  }

  /* Construct the userdata to be supplied to the callback */
  userdata = &(nr_segment_userdata_t){
         .txn = txn,
         .segment_names = segment_names,
         .success = true,
         .trace = {
           .buf = buf,
           .sample = trace_set,
           .current_path = nr_vector_create(12, NULL, NULL),
           .sampled_ancestors_with_child = nr_set_create(),
         },
         .spans = {
           .events = span_events,
           .sample = span_set,
         },
  };
  nr_stack_init(&userdata->spans.parent_ids, 12);

  nr_segment_iterate(
      root, (nr_segment_iter_t)nr_segment_traces_stot_iterator_callback,
      userdata);

  nr_set_destroy(&(userdata->trace.sampled_ancestors_with_child));
  nr_vector_destroy(&(userdata->trace.current_path));
  nr_stack_destroy_fields(&userdata->spans.parent_ids);

  return userdata->success;
}

void nr_segment_traces_create_data(
    const nrtxn_t* txn,
    nrtime_t duration,
    nr_segment_tree_sampling_metadata_t* metadata,
    const nrobj_t* agent_attributes,
    const nrobj_t* user_attributes,
    const nrobj_t* intrinsics,
    bool create_trace,
    bool create_spans) {
  nrbuf_t* buf = NULL;
  bool print_success;
  nr_vector_t* span_events = NULL;
  nrpool_t* segment_names;

  if ((NULL == txn) || (0 == txn->segment_count) || (0 == duration)
      || NULL == metadata || NULL == metadata->out
      || (NULL != metadata->trace_set
          && NR_MAX_SEGMENTS < nr_set_size(metadata->trace_set))) {
    return;
  }

  if (create_trace) {
    buf = nr_buffer_create(4096 * 8, 4096 * 4);
  }

  if (create_spans) {
    size_t app_span_event_limit = (size_t)txn->app_limits.span_events;
    size_t vector_size = (txn->segment_count > app_span_event_limit)
                             ? app_span_event_limit
                             : txn->segment_count;
    span_events
        = nr_vector_create(vector_size, nr_vector_span_event_dtor, NULL);
  }

  segment_names = nr_string_pool_create();

  /*
   * Here we create a JSON string which will be eventually be compressed,
   * encoded, and embedded into the final trace JSON structure for the
   * New Relic backend.
   */
  nr_buffer_add(buf, "[", 1);
  nr_buffer_add(buf, "[", 1);
  nr_buffer_add(buf, "0.0", 1); /* Unused timestamp. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "{}", 2); /* Unused:  Formerly request-parameters. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "{}", 2); /* Unused:  Formerly custom-parameters. */
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);
  nr_buffer_add(buf, "0", 1);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_write_uint64_t_as_text(buf, duration / NR_TIME_DIVISOR_MS);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "\"ROOT\"", 6);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "{}", 2);
  nr_buffer_add(buf, ",", 1);
  nr_buffer_add(buf, "[", 1);

  print_success = nr_segment_traces_json_print_segments(
      buf, span_events, metadata->trace_set, metadata->span_set, txn,
      txn->segment_root, segment_names);

  if (!print_success) {
    nrl_warning(NRL_SEGMENT,
                "Segment iteration failed; no trace or span events will be "
                "generated for this transaction");
    nr_string_pool_destroy(&segment_names);
    nr_buffer_destroy(&buf);
    return;
  }

  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, ",", 1);
  {
    nrobj_t* hash = nro_new_hash();

    if (agent_attributes) {
      nro_set_hash(hash, "agentAttributes", agent_attributes);
    }
    if (user_attributes) {
      nro_set_hash(hash, "userAttributes", user_attributes);
    }
    if (intrinsics) {
      nro_set_hash(hash, "intrinsics", intrinsics);
    }

    nr_buffer_add(buf, "{", 1);
    add_attribute_hash_to_buffer(buf, hash);
    nr_buffer_add(buf, "}", 1);

    nro_delete(hash);
  }
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, ",", 1);
  {
    char* js = nr_string_pool_to_json(segment_names);

    nr_buffer_add(buf, js, nr_strlen(js));
    nr_free(js);
  }
  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "\0", 1);
  if (create_trace) {
    metadata->out->trace_json = nr_strdup((const char*)nr_buffer_cptr(buf));
  } else {
    metadata->out->trace_json = NULL;
  }
  metadata->out->span_events = span_events;

  nr_string_pool_destroy(&segment_names);
  nr_buffer_destroy(&buf);

  return;
}
