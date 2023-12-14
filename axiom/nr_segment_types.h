#ifndef NR_SEGMENT_TYPES_HDR
#define NR_SEGMENT_TYPES_HDR

#include "nr_datastore_instance.h"
#include "nr_exclusive_time.h"
#include "nr_attributes.h"
#include "nr_span_event.h"
#include "util_metrics.h"
#include "util_minmax_heap.h"
#include "util_object.h"
#include "util_set.h"
#include "util_vector.h"

/*
 * Forward declaration of nr_segment_t, since we have a circular dependency with
 *  nr_segment.h.
 */
typedef struct _nr_segment_t nr_segment_t;
typedef struct _nrtxn_t nrtxn_t;

/*
 * The data structure for packed children, holding an array of children and the
 * number of elements in the array.
 */
typedef struct {
  size_t count;
  nr_segment_t* elements[NR_SEGMENT_CHILDREN_PACKED_LIMIT];
} nr_segment_packed_children_t;

/*
 * The children structure. If `is_packed` is true the union is used as packed,
 * otherwise it is used as vector.
 */
typedef struct {
  bool is_packed;
  union {
    nr_vector_t vector;
    nr_segment_packed_children_t packed;
  };
} nr_segment_children_t;

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
  size_t child_ix;
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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */

  /*
   * Because of the access to the segment is now split between functions, we
   * need to pass a certain amount of data between the functions that use the
   * segment.
   */
  void* wraprec; /* wraprec, if one is associated with this segment, to reduce
                    wraprec lookups */
#endif

} nr_segment_t;
#endif
