/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions used to access and change trees of segments.
 */

#ifndef NR_SEGMENT_TREE_H
#define NR_SEGMENT_TREE_H

#include "nr_segment.h"
#include "nr_segment_traces.h"
#include "nr_txn.h"

#include "util_set.h"

/*
 * To assemble the transaction trace and the array of span events, the axiom
 * library must iterate over the tree of segments. This struct contains the
 * input metadata and result storage for that operation.
 */
typedef struct {
  nr_set_t* trace_set;
  nr_set_t* span_set;
  nrtxnfinal_t* out;
} nr_segment_tree_sampling_metadata_t;

/*
 * Purpose : Traverse all the segments in the tree.  If a transaction trace is
 *           merited, assemble the transaction trace JSON for the highest
 *           priority segments and place it in the result struct.
 *
 * Params  : 1. A pointer to the transaction.
 *           2. The limit of segments permitted in the trace.
 *           3. The limit of span events permitted.
 *           4. An optional callback that will be invoked once the total time
 *              has been calculated, but before the trace and span events are
 *              generated.
 *           5. Optional userdata to provide to the callback.
 *
 * Returns : The generated trace, span events, and total time. If the
 *           transaction did not generate a trace or span events, the relevant
 *           field will be NULL.
 */
extern nrtxnfinal_t nr_segment_tree_finalise(
    nrtxn_t* txn,
    const size_t trace_limit,
    const size_t span_limit,
    void (*total_time_cb)(nrtxn_t* txn, nrtime_t total_time, void* userdata),
    void* callback_userdata);

/*
 * Purpose : Return a pointer to the closest sampled ancestor of the
 *           provided segment.
 *
 * Params  : 1. The set of sampled segment.
 *           2. The starting place, the segment who's ancestors we want to
 *              learn about.
 *
 * Return  : The nearest ancestor of the passed in segment or NULL if none
 *           found.  The ancestor must be sampled, meaning that it is
 *           contained in the provided set.
 */
extern nr_segment_t* nr_segment_tree_get_nearest_sampled_ancestor(
    nr_set_t* sampled_set,
    const nr_segment_t* segment);

#endif
