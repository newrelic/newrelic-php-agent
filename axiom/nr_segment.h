/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains data types and functions for dealing with the segments of
 * a transaction.  Historically, segments have also been called nodes, or
 * trace nodes and these three words are often used interchangeably in this
 * repository.
 *
 * It defines the data types and functions used to build up the multiple
 * segments comprising a single transaction. Segments may by created
 * automatically by the agent or programmatically, by means of customer API
 * calls.
 */
#ifndef NR_SEGMENT_HDR
#define NR_SEGMENT_HDR

typedef struct _nr_segment_t nr_segment_t;
typedef struct _nrtxn_t nrtxn_t;

#include "nr_datastore_instance.h"
#include "nr_exclusive_time.h"
#include "nr_segment_children.h"
#include "nr_span_event.h"
#include "nr_txn.h"
#include "util_metrics.h"
#include "util_minmax_heap.h"
#include "util_object.h"
#include "util_set.h"
#include "util_vector.h"

typedef enum _nr_segment_type_t {
  NR_SEGMENT_CUSTOM,
  NR_SEGMENT_DATASTORE,
  NR_SEGMENT_EXTERNAL
} nr_segment_type_t;

/*
 * The first iteration over the tree will put segments into two heaps: one for
 * span events, and the other for traces. It keeps a running total of the
 * transaction's total time, which is the sum of all exclusive time.
 *
 * This struct is used to pass in the two heaps, along with the field to track
 * the total time.
 */
typedef struct {
  nr_minmax_heap_t* span_heap;
  nr_minmax_heap_t* trace_heap;
  nrtime_t total_time;
  nr_exclusive_time_t* main_context;
} nr_segment_tree_to_heap_metadata_t;

/*
 * Segment Coloring
 *
 * The C Agent API gives customers the ability to arbitrarily parent a segment
 * with any other segment. As a result, it is possible to introduce a cycle
 * into the tree. To avoid infinite regress during the recursive traversal of
 * the tree, the nodes are colored during traversal to indicate that they've
 * already been traversed.
 */
typedef enum _nr_segment_color_t {
  NR_SEGMENT_WHITE,
  NR_SEGMENT_GREY
} nr_segment_color_t;

/*
 * Segment priority indicators
 *
 * These go into the priority bitfield in the nr_segment_t struct and can be
 * set via nr_segment_set_priority_flag. The higher the value of the priority
 * field, the higher the likelihood that the span created from the segment will
 * be kept.
 *
 * NR_SEGMENT_PRIORITY_ROOT indicates that the segment in question is a root
 * segment.
 *
 * NR_SEGMENT_PRIORITY_DT indicates that the segment's id is included in an
 * outbound DT payload.
 *
 * NR_SEGMENT_PRIORITY_LOG indicates that the segment's id is included in an
 * log payload.
 *
 * NR_SEGMENT_PRIORITY_ATTR indicates that the segment has user attributes
 * added to it.
 */
#define NR_SEGMENT_PRIORITY_ROOT (1 << 16)
#define NR_SEGMENT_PRIORITY_DT (1 << 15)
#define NR_SEGMENT_PRIORITY_LOG (1 << 14)
#define NR_SEGMENT_PRIORITY_ATTR (1 << 13)

typedef struct _nr_segment_datastore_t {
  char* component; /* The name of the database vendor or driver */
  char* sql;
  char* sql_obfuscated;
  char* input_query_json;
  char* backtrace_json;
  char* explain_plan_json;
  nr_datastore_instance_t instance;
} nr_segment_datastore_t;

typedef struct _nr_segment_external_t {
  char* transaction_guid;
  char* uri;
  char* library;
  char* procedure; /* Also known as method. */
  uint64_t status;
} nr_segment_external_t;

typedef struct _nr_segment_metric_t {
  char* name;
  bool scoped;
} nr_segment_metric_t;

typedef struct _nr_segment_error_t {
  char* error_message; /* The error message that will appear on a span event. */
  char* error_class;   /* The error class that will appear on a span event. */
} nr_segment_error_t;

/*
 * Type specific fields.
 *
 * The union type can only hold one struct at a time. This ensures that we
 * will not reserve memory for variables that are not applicable for this type
 * of node. Example: A datastore node will not need to store a method and an
 * external node will not need to store a component.
 *
 * You must check the nr_segment_type to determine which struct is being used.
 */
typedef union {
  nr_segment_datastore_t datastore;
  nr_segment_external_t external;
} nr_segment_typed_attributes_t;

typedef struct _nr_segment_t {
  nr_segment_type_t type;
  nrtxn_t* txn;

  /* Tree related stuff. */
  nr_segment_t* parent;
  nr_segment_children_t children;
  size_t child_ix; /* index of this segment in its parent->children vector */
  nr_segment_color_t color;

  /* Generic segment fields. */

  /* The start_time and stop_time of a segment are relative times.  For each
   * field, a value of 0 is equal to the absolute start time of the transaction.
   */

  nrtime_t start_time; /* Start time for node, relative to the start of
                          the transaction. */
  nrtime_t stop_time;  /* Stop time for node, relative to the start of the
                          transaction. */

  int name;             /* Node name (pooled string index) */
  int async_context;    /* Execution context (pooled string index) */
  char* id;             /* Node id.
            
                           If this is NULL, a new id will be created when a
                           span event is created from this trace node.
            
                           If this is not NULL, this id will be used for
                           creating a span event from this trace node. This
                           id set indicates that the node represents an
                           external segment and the id of the segment was
                           use as current span id in an outgoing DT payload.
                          */
  nr_vector_t* metrics; /* Metrics to be created by this segment. */
  nr_exclusive_time_t* exclusive_time; /* Exclusive time.
                                       This is only calculated after the
                                       transaction has ended; before then, this
                                       will be NULL. */
  nr_attributes_t* attributes;         /* User attributes */
  nr_attributes_t*
      attributes_txn_event; /* Transaction event custom user attributes */
  int priority; /* Used to determine which segments are preferred for span event
                   creation */
  nr_segment_typed_attributes_t* typed_attributes; /* Attributes specific to
                                                      external or datastore
                                                      segments. */
  nr_segment_error_t* error; /* segment error attributes */
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  void* execute_data;

#elif ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */

  /*
   * Because of the access to the segment is now split between functions, we
   * need to pass a certain amount of data between the functions that use the
   * segment.
   */
  void* wraprec; /* wraprec, if one is associated with this segment, to reduce
                    wraprec lookups */
  int is_exception_handler; /* 1 if segment is associated with exception
                               handler, 0 otherwise */
#endif

} nr_segment_t;

/*
 * Type declarations for iterators.
 */
typedef void (*nr_segment_post_iter_t)(nr_segment_t* segment, void* userdata);

typedef struct _nr_segment_iter_return_t {
  nr_segment_post_iter_t post_callback;
  void* userdata;
} nr_segment_iter_return_t;

static const nr_segment_iter_return_t NR_SEGMENT_NO_POST_ITERATION_CALLBACK
    = {.post_callback = NULL, .userdata = NULL};

typedef nr_segment_iter_return_t (*nr_segment_iter_t)(nr_segment_t* segment,
                                                      void* userdata);

/*
 * Purpose : Allocate and start a segment within a transaction's trace.
 *
 * Params  : 1. The current transaction.
 *           2. The pointer of this segment's parent, NULL if no explicit parent
 *              is known at the time of this call.  A non-NULL value is typical
 *              for API calls that support asynchronous calls.  A NULL value is
 *              typical for instrumentation of synchronous calls.
 *           3. The async_context to be applied to the segment, or NULL to
 *              indicate that the segment is not asynchronous.
 *
 * Note    : At the time of this writing, if an explicit parent is supplied
 *           then an async_context must also be supplied.  If parent is NULL
 *           and async is not NULL, or vice-versa, it can lead to undefined
 *           behavior in the agent.
 *
 * Returns : A segment.
 */
extern nr_segment_t* nr_segment_start(nrtxn_t* txn,
                                      nr_segment_t* parent,
                                      const char* async_context);

/*
 * Purpose : Start an already allocated segment.
 *
 * Params  : 1. The allocated segment.
 *           2. The current transaction.
 *           3. The pointer of this segment's parent, NULL if no explicit parent
 *              is known at the time of this call.  A non-NULL value is typical
 *              for API calls that support asynchronous calls.  A NULL value is
 *              typical for instrumentation of synchronous calls.
 *           4. The async_context to be applied to the segment, or NULL to
 *              indicate that the segment is not asynchronous.
 *
 * Note    : At the time of this writing, if an explicit parent is supplied
 *           then an async_context must also be supplied.  If parent is NULL
 *           and async is not NULL, or vice-versa, it can lead to undefined
 *           behavior in the agent.
 *
 * Warning : This function should only be used with segments that have been
 *           previously allocated and initialized via nr_segment_start and then
 *           de-initialized via nr_segment_deinit_impl.
 *
 * Returns : false if a NULL segment was supplied, true otherwise.
 */
extern bool nr_segment_init(nr_segment_t* segment,
                            nrtxn_t* txn,
                            nr_segment_t* parent,
                            const char* async_context);
/*
 * Purpose : Destroy the fields within the given segment, without freeing the
 *           segment itself.
 *
 * Params  : 1. The segment to destroy the fields of.
 */
extern void nr_segment_destroy_fields(nr_segment_t* segment);

/*
 * Purpose : Create a span event from a segment.
 *
 * Params  : 1. The segment to use.
 *
 * Returns : A span event, which must be destroyed with nr_span_event_destroy(),
 *           or NULL on error.
 *
 * Note    : This function will always fail for active segments, or when used
 *           with transactions with DT and/or span events disabled.
 */
extern nr_span_event_t* nr_segment_to_span_event(nr_segment_t* segment);

/*
 * Purpose : Mark the segment as being a custom segment.
 *
 * Params  : 1. The pointer to the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_custom(nr_segment_t* segment);

/*
 * Purpose : Mark the segment as being a datastore segment.
 *
 * Params  : 1. The pointer to the segment.
 *           2. The datastore attributes, which will be copied into the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_datastore(nr_segment_t* segment,
                                     const nr_segment_datastore_t* datastore);

/*
 * Purpose : Mark the segment as being an external segment.
 *
 * Params  : 1. The pointer to the segment.
 *           2. The external attributes, which will be copied into the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_external(nr_segment_t* segment,
                                    const nr_segment_external_t* external);
/*
 * Purpose : Add a child to a segment.
 *
 * Params  : 1. The pointer to the parent segment.
 *           2. The pointer to the child segment.
 *
 * Notes   : If a segment, s1, is a parent of another segment, s2, that means
 *           that the instrumented code represented by s1 called into s2.
 *
 *           nr_segment_add_child() calls nr_segment_set_parent().  By itself,
 *           this function may not offer great utility, but there are times
 *           when it's easier to reason about adding a child and there are times
 *           when it's easier to reason about re-parenting a child.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_add_child(nr_segment_t* parent, nr_segment_t* child);

/*
 * Purpose : Add a metric to a segment.
 *
 * Params  : 1. The segment.
 *           2. The name of the metric to create based on the segment duration.
 *           3. True to make a scoped metric; false to create an unscoped
 *              metric.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_add_metric(nr_segment_t* segment,
                                  const char* name,
                                  bool scoped);

/*
 * Purpose : Set the name of a segment.
 *
 * Params  : 1. The pointer to the segment to be named.
 *           2. The name to be applied to the segment.
 *
 * Note    : The segment's name may be available at the start of the segment, as
 *           in cases of newrelic_start_segment(), or not until the segment's
 *           end, in cases of PHP instrumentation.  Make setting the segment
 *           name separate from starting or ending the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_name(nr_segment_t* segment, const char* name);

/*
 * Purpose : Set the parent of a segment.
 *
 * Params  : 1. The pointer to the segment to be parented.
 *           2. The pointer to the segment to become the new parent.
 *
 * Returns : true if successful, false otherwise. If the target segment
 *           is an ancestor of the target parent, the function will return
 *           false to prevent a cycle from being created.
 */
extern bool nr_segment_set_parent(nr_segment_t* segment, nr_segment_t* parent);

/*
 * Purpose : Set the timing of a segment.
 *
 * Params  : 1. The pointer to the segment to be retimed.
 *           2. The new start time for the segment, in microseconds since
 *              the start of the transaction.
 *           3. The new duration for the segment.
 *
 * Notes   : A start value of 0 means that the segment started at the same
 *           time as its transaction.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_timing(nr_segment_t* segment,
                                  nrtime_t start,
                                  nrtime_t duration);

/*
 * Purpose  : Add a user attribute to a segment.
 *
 * Params   : 1. The pointer to the segment.
 *            2. The attribute destination (see nr_attributes.h for possible
 *               values).
 *            3. The name of the attribute to be added.
 *            4. The value of the attribute to be added.
 * Returns  : true if successful, false otherwise.
 */
extern bool nr_segment_attributes_user_add(nr_segment_t* segment,
                                           uint32_t destination,
                                           const char* name,
                                           const nrobj_t* value);

/*
 * Purpose  : Add a span user attribute to a segment.
 *
 * Params   : 1. The pointer to the segment.
 *            2. The attribute destination (see nr_attributes.h for possible
 *               values).
 *            3. The name of the attribute to be added.
 *            4. The value of the attribute to be added.
 *
 * Returns  : true if successful, false otherwise.
 */
extern bool nr_segment_attributes_user_span_event_add(nr_segment_t* segment,
                                                      uint32_t destination,
                                                      const char* name,
                                                      const nrobj_t* value);

/*
 * Purpose  : Add a transaction user attribute to a segment.
 *
 * Params   : 1. The pointer to the segment.
 *            2. The attribute destination (see nr_attributes.h for possible
 *               values).
 *            3. The name of the attribute to be added.
 *            4. The value of the attribute to be added.
 *
 * Returns  : true if successful, false otherwise.
 */
extern bool nr_segment_attributes_user_txn_event_add(nr_segment_t* segment,
                                                     uint32_t destination,
                                                     const char* name,
                                                     const nrobj_t* value);

/*
 * Purpose : End a segment within a transaction's trace.
 *
 * Params  : 1. The address of the segment to be ended.
 *
 * Returns : true if successful, false otherwise.
 *
 * Notes   : If nr_segment_set_timing() has been called, then the previously
 *           set duration will not be overriden by this function.
 *
 *           A segment can only be ended when its corresponding transaction
 *           is active.  Ending a segment after its transaction has ended
 *           results in undefined behavior.
 */
extern bool nr_segment_end(nr_segment_t** segment);

/*
 * Purpose : Destroy the fields within the given segment, without freeing the
 *           segment itself.
 *
 * Params  : 1. The segment to destroy the fields of.
 *
 */
extern void nr_segment_destroy_fields(nr_segment_t* segment);

/*
 * Purpose : Iterate over the segments in a tree of segments.
 *
 * Params  : 1. A pointer to the root.
 *           2. A callback that will be invoked for each segment before that
 *              segment's children have been traversed. This callback may
 *              return a structure registering a callback to be invoked after
 *              the segment's children have been traversed, or
 *              NR_SEGMENT_NO_POST_ITERATION_CALLBACK to disable any
 *              post-traversal callback.
 *           3. Optional userdata for the iterators.
 */
extern void nr_segment_iterate(nr_segment_t* root,
                               nr_segment_iter_t callback,
                               void* userdata);

/* Purpose : Create a heap of segments.
 *
 * Params  : 1. The bound for the heap, 0 if unbounded.
 *           2. The comparator function for the heap to use.
 *
 * Returns : A pointer to the newly-created heap.
 */
nr_minmax_heap_t* nr_segment_heap_create(ssize_t bound,
                                         nr_minmax_heap_cmp_t comparator);

/*
 * Purpose : Compare two segments.
 *
 * Params : 1. A pointer to a, the first segment for comparison.
 *          2. A pointer to b, the second segment for comparison.
 *          3. An unused pointer required by nr_minmax_heap_t.
 *
 * Returns : -1 if the duration of a is less than the duration of b.
 *            0 if the durations are equal.
 *            1 if the duration of a is greater than the duration of b.
 *
 * Note    : This is the comparison function required for
 *           creating a minmax heap of segments.
 */
extern int nr_segment_wrapped_duration_comparator(const void* a,
                                                  const void* b,
                                                  void* userdata NRUNUSED);

/*
 * Purpose : Compare the span priority of two segments.
 *
 * Params : 1. A pointer to a, the first segment for comparison.
 *          2. A pointer to b, the second segment for comparison.
 *          3. An unused pointer required by nr_minmax_heap_t.
 *
 * Returns : -1 if the span priority of a is less than the span priority of b.
 *            0 if the span priorities are equal.
 *            1 if the span priority of a is greater than the span priority of
 *              b.
 *
 * Note    : This is a comparison function required for creating a minmax heap
 *           of segments.
 *
 *           The segment with the higher value of its priority field is
 *           given priority. If both priority values are the same, the segment
 *           with the longer duration is given priority.
 */
extern int nr_segment_wrapped_span_priority_comparator(const void* a,
                                                       const void* b,
                                                       void* userdata NRUNUSED);

/*
 * Purpose : Place an nr_segment_t pointer into a buffer.
 *             or "segments to trace".
 *
 * Params  : 1. The segment pointer to print as JSON to the buffer.
 *           2. A void* pointer to be recast as the pointer to the
 *              nr_segment_tree_to_heap_metadata_t a custom collection of data
 *              required to print one segment's worth of JSON into the buffer.
 */
extern nr_segment_iter_return_t nr_segment_stot_iterator_callback(
    nr_segment_t* segment,
    void* userdata);

/*
 * Purpose : Given a root of a tree of segments, create a heap of segments.
 *
 * Params  : 1. A pointer to the root segment.
 *           2. A pointer to the metadata for this pass.
 */
extern void nr_segment_tree_to_heap(
    nr_segment_t* root,
    nr_segment_tree_to_heap_metadata_t* metadata);

/*
 * Purpose : Given a heap of segments, create a set containing the highest
 *           priority segments.
 *
 * Params  : 1. The heap.
 *           2. The set to populate.
 */
extern void nr_segment_heap_to_set(nr_minmax_heap_t* heap, nr_set_t* set);

/*
 * Purpose : Free a tree of segments.
 *
 * Params  : 1. A pointer to the root.
 *
 * WARNING : This should only be called during transaction destruction.
 *
 */
extern void nr_segment_destroy_tree(nr_segment_t* root);

/*
 * Purpose : Discard and free a single segment.
 *
 * Params  : 1. The address of a segment.
 *
 * Returns : True if the segment was successfully discarded.
 *
 * Notes   : Discarding a segment removes a single segment from the segment
 *           tree. Children of the discarded segment are re-parented with the
 *           parent of the segment.
 *
 *           A segment without a parent (a root segment) cannot be discarded.
 *
 *           A segment that has been ended with nr_segment_end() cannot be
 *           discarded, as it may exist in the transaction's segment heap.
 *           Doing so will result in undefined behaviour.
 */
extern bool nr_segment_discard(nr_segment_t** segment);

/*
 * Purpose : Ensure the segment has an ID.
 *
 *           This function is guaranteed to return an ID if span events will be
 *           created for the given transaction, otherwise it can return NULL.
 *
 * Params  : 1. A pointer to a segment.
 *           2. The transaction.
 *
 * Returns : The ID of the segment or NULL.
 */
extern char* nr_segment_ensure_id(nr_segment_t* segment, const nrtxn_t* txn);

/*
 * Purpose : Set a segment priority flag
 *
 *           This will influence the likelihood with which a span event will be
 *           created for a segment.
 *
 * Params  : 1. A pointer to a segment
 *           2. One of the NR_SEGMENT_PRIORITY_* flags
 *
 * Notes   : Multiple flags can be set for a single segment, either by
 *           multiple calls to this function or by chaining flags with the `|`
 *           operator.
 */
extern void nr_segment_set_priority_flag(nr_segment_t* segment, int flag);

/*
 * Purpose : Get a segment priority flag
 *
 * Params  : 1. A pointer to a segment
 *
 * Returns : Integer value of the segment's priority flag
 */
extern int nr_segment_get_priority_flag(nr_segment_t* segment);

/*
 * Purpose : Set the error attributes on a segment.
 *
 * Params  : 1. The pointer to the segment.
 *           2. The error message that will be added.
 *           3. The error class that will be added.
 */
extern void nr_segment_set_error(nr_segment_t* segment,
                                 const char* error_message,
                                 const char* error_class);

/*
 * Purpose : Record an uncaught exception on the segment.
 *
 * Params  : 1. The pointer to the segment.
 *           2. The error message that will be added.
 *           3. The error class that will be added.
 */
extern void nr_segment_record_exception(nr_segment_t* segment,
                                        const char* error_message,
                                        const char* error_class);

/*
 * Purpose : Gets the child_ix of a segment.
 *
 * Paramas : 1. The pointer to the segment
 *
 * Returns : the child_ix, or -1 if passed NULL
 */
extern ssize_t nr_segment_get_child_ix(const nr_segment_t*);

/*
 * Purpose : Sets the child_ix of a segment.
 *
 * Paramas : 1. The pointer to the segment
 *           2. the child_ix to set
 */
extern void nr_segment_set_child_ix(nr_segment_t*, size_t);

#endif /* NR_SEGMENT_HDR */
