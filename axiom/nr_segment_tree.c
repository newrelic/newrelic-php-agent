/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_segment_traces.h"
#include "nr_segment_tree.h"

nrtxnfinal_t nr_segment_tree_finalise(nrtxn_t* txn,
                                      const size_t trace_limit,
                                      const size_t span_limit,
                                      void (*total_time_cb)(nrtxn_t* txn,
                                                            nrtime_t total_time,
                                                            void* userdata),
                                      void* callback_userdata) {
  bool should_save_trace = false;
  bool should_sample_trace = false;
  bool should_save_spans = false;
  bool should_sample_spans = false;
  nrtxnfinal_t result = {
      .trace_json = NULL,
      .span_events = NULL,
      .total_time = 0,
  };
  nr_segment_tree_to_heap_metadata_t first_pass_metadata = {
      .trace_heap = NULL,
      .span_heap = NULL,
      .total_time = 0,
      .main_context = NULL,
  };
  nrtime_t duration;

  if (NULL == txn || NULL == txn->segment_root) {
    return result;
  }

  duration = nr_txn_duration(txn);

  should_save_trace
      = (trace_limit > 0) && nr_txn_should_save_trace(txn, duration);
  should_sample_trace = txn->segment_count > trace_limit;

  should_save_spans = (span_limit > 0) && nr_txn_should_create_span_events(txn)
                      && NULL == txn->span_queue;
  should_sample_spans = txn->segment_count > span_limit;

  if (should_save_spans && should_sample_spans) {
    first_pass_metadata.span_heap = nr_segment_heap_create(
        span_limit, nr_segment_wrapped_span_priority_comparator);
  }
  if (should_save_trace && should_sample_trace) {
    first_pass_metadata.trace_heap = nr_segment_heap_create(
        trace_limit, nr_segment_wrapped_duration_comparator);
  }

  /*
   * We'll use an exclusive time structure to calculate how long the main
   * context was blocked, if that was requested for this transaction.
   */
  if (txn->options.discount_main_context_blocking) {
    first_pass_metadata.main_context
        = nr_exclusive_time_create(txn->segment_count, 0, duration);
  }

  /*
   * Do the first pass over the tree: we need to generate the heaps tracking the
   * segments that will be used in any transaction trace or span event
   * reservoir and calculate the total time for the transaction.
   */
  nr_segment_tree_to_heap(txn->segment_root, &first_pass_metadata);

  /*
   * We always need to set the total time.
   */
  result.total_time = first_pass_metadata.total_time;

  /*
   * If the discount main context blocking option was set, then we need to
   * remove the time the main context was blocked from the total time.
   */
  if (txn->options.discount_main_context_blocking) {
    /*
     * This looks more complicated than it should be because we're abusing the
     * exclusive time type a little here: what it calculates normally is the
     * time a segment was executing, whereas we actually want the time the fake
     * segment wasn't executing. Fortunately, we can calculate that by
     * subtracting the "exclusive time" (ie time on the main context) from the
     * transaction duration.
     */
    const nrtime_t main_blocked
        = duration
          - nr_exclusive_time_calculate(first_pass_metadata.main_context);

    result.total_time -= main_blocked;
    nr_exclusive_time_destroy(&first_pass_metadata.main_context);
  }

  /*
   * If the caller wants an opportunity to do things to the transaction with the
   * total time before the trace or span events are generated, now is the time.
   */
  if (total_time_cb) {
    (total_time_cb)(txn, result.total_time, callback_userdata);
  }

  /*
   * Now we do a second pass if needed. If we don't need to generate a trace or
   * span events, then there's no need.
   */
  if (should_save_trace || should_save_spans) {
    nrobj_t* agent_attributes;
    nrobj_t* user_attributes;
    nr_segment_tree_sampling_metadata_t metadata = {
        .trace_set = NULL,
        .span_set = NULL,
        .out = &result,
    };

    if (should_sample_trace) {
      /* Prepare for the second pass of the tree; convert the heap to a set. */
      metadata.trace_set = nr_set_create();
      nr_segment_heap_to_set(first_pass_metadata.trace_heap,
                             metadata.trace_set);
    }

    if (should_sample_spans) {
      /* Prepare for the second pass of the tree; convert the heap to a set. */
      metadata.span_set = nr_set_create();
      nr_segment_heap_to_set(first_pass_metadata.span_heap, metadata.span_set);
    }

    agent_attributes = nr_attributes_agent_to_obj(
        txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);
    user_attributes = nr_attributes_user_to_obj(
        txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);

    nr_segment_traces_create_data(txn, duration, &metadata, agent_attributes,
                                  user_attributes, txn->intrinsics,
                                  should_save_trace, should_save_spans);
    result.trace_json = metadata.out->trace_json;
    result.span_events = metadata.out->span_events;

    nro_delete(agent_attributes);
    nro_delete(user_attributes);

    nr_set_destroy(&metadata.trace_set);
    nr_set_destroy(&metadata.span_set);
    nr_minmax_heap_destroy(&first_pass_metadata.trace_heap);
    nr_minmax_heap_destroy(&first_pass_metadata.span_heap);
  }

  return result;
}

nr_segment_t* nr_segment_tree_get_nearest_sampled_ancestor(
    nr_set_t* sampled_set,
    const nr_segment_t* segment) {
  if (NULL == segment || NULL == sampled_set || NULL == segment->txn) {
    return NULL;
  } else {
    nr_segment_t* root = segment->txn->segment_root;
    nr_segment_t* current = segment->parent;

    while (NULL != current) {
      if (nr_set_contains(sampled_set, current)) {
        return current;
      }
      // If a cycle was created in the tree it will be lost unless root is given
      // a parent. This clause will prevent infinite looping.
      if (current == root) {
        return NULL;
      }
      current = current->parent;
    }
  }

  return NULL;
}
