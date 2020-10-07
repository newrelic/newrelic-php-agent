/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SEGMENT_TRACES_HDR
#define NR_SEGMENT_TRACES_HDR

#include "nr_segment.h"
#include "nr_segment_tree.h"
#include "nr_span_event.h"
#include "util_stack.h"
#include "util_set.h"

/*
 * Segment Iteration Userdata
 *
 * To parametrically iterate over a tree of segments, the requisite callback
 * function takes two parameters, a pointer to a particular segment and a
 * pointer to userdata. Traversing a tree of segments creates both the trace
 * output and the span event output, thus including the original segment tree,
 * three potentially different trees have to be juggled.
 *
 * The userdata is structured to accommodate that: the topmost struct holds data
 * necessary to both the trace and span event creation. Data specific to either
 * trace or span event creation is stored in dedicated nested structs.
 */
typedef struct {
  nrbuf_t* buf;     /* The buffer to print JSON into */
  nr_set_t* sample; /* The set of segments that should be added to the trace */
  nr_vector_t* current_path; /* The path of ancestor segments that were added to
                                the trace; used to determine parents and to
                                determine state in the post traversal callback
                              */
  nr_set_t* sampled_ancestors_with_child; /* The set of sampled ancestors that
                                             have at least one child; used to
                                             determine siblings */
} nr_segment_userdata_trace_t;

typedef struct {
  nr_vector_t* events; /* The output vector to add span events to */
  nr_set_t* sample; /* The set of segments that should be added to the list of
                       spans */
  nr_stack_t parent_ids; /* The path of ancestor span IDs */
} nr_segment_userdata_spans_t;

typedef struct _nr_segment_userdata_t {
  const nrtxn_t* txn;      /* The transaction, its string pool, and its pointer
                              to the root segment */
  nrpool_t* segment_names; /* The string pool for the transaction trace */
  bool success;            /* Was the call successful? */
  nr_segment_userdata_trace_t trace; /* Data relevant for trace generation */
  nr_segment_userdata_spans_t
      spans; /* Data relevant for span event generation */
} nr_segment_userdata_t;

/*
 * Purpose : Create the internals of the transaction trace JSON expected by the
 *           New Relic backend.  If a segment is a member of
 *           metadata->trace_set, it is added to the transaction trace JSON. If
 *           metadata->trace_set is NULL, all segments are added.
 *
 *           Furthermore, populate the span event vector in
 *           metadata->out.span_events.  If a segment is a member of
 *           metadata->trace_set, a span event is generated and added to the
 *           output vector.  If metadata->span_set is NULL, all span events for
 *           all segments are added.
 *
 * Params  : 1. The transaction.
 *           2. The duration.
 *           3. The collection of metadata input and storage for placing
 *              the resulting trace JSON.
 *           4. A hash representing the agent attributes.
 *           5. A hash representing the user attributes.
 *           6. A hash representing intrinsics.
 *           7. true if trace should be generated.
 *           8. true if spans should be generated.
 */
void nr_segment_traces_create_data(
    const nrtxn_t* txn,
    nrtime_t duration,
    nr_segment_tree_sampling_metadata_t* metadata,
    const nrobj_t* agent_attributes,
    const nrobj_t* user_attributes,
    const nrobj_t* intrinsics,
    bool create_trace,
    bool create_spans);

/*
 * Purpose : Recursively print segments to a buffer in json format.
 *
 * Params  : 1. The buffer.
 *           2. An output vector to store generated span events.
 *           3. The set of segments that should be in the trace. NULL if all
 *              segments should be in the trace.
 *           4. The set of segments that should be span events. NULL if all
 *              segments should be span events.
 *           5. The transaction, largely for the transaction's string pool and
 *              async duration.
 *           6. The root pointer for the tree of segments.
 *           7. A string pool that the node names will be put into.  This string
 *              pool is included in the data json after the nodes:  It is used
 *              to minimize the size of the JSON.
 *
 * Returns : True on success; false otherwise.
 */
bool nr_segment_traces_json_print_segments(nrbuf_t* buf,
                                           nr_vector_t* span_events,
                                           nr_set_t* trace_set,
                                           nr_set_t* span_set,
                                           const nrtxn_t* txn,
                                           nr_segment_t* root,
                                           nrpool_t* segment_names);

/*
 * Purpose : Place an nr_segment_t pointer into a buffer.
 *
 * Params  : 1. The segment pointer to print as JSON to the buffer.
 *           2. A void* pointer to be recast as the pointer to the
 *              nr_segment_userdata_t a custom collection of data
 *              required to print one segment's worth of JSON into
 *              the buffer.
 */
extern nr_segment_iter_return_t nr_segment_traces_stot_iterator_callback(
    nr_segment_t* segment,
    void* userdata);
#endif
