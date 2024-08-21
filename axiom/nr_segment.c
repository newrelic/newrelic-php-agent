/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_distributed_trace.h"
#include "nr_guid.h"
#include "nr_segment_private.h"
#include "nr_segment.h"
#include "nr_segment_traces.h"
#include "nr_txn.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_slab.h"
#include "util_string_pool.h"
#include "util_strings.h"
#include "util_time.h"

/*
 * Custom struct to pass the span event and the attribute count in order to
 * maintain the required number of user attributes even when combining the span
 * user attributes and transaction user attributes.
 */
struct _nr_span_event_and_counter_t {
  nr_span_event_t* event;
  int counter;
};

typedef struct _nr_span_event_and_counter_t nr_span_event_and_counter_t;

/*
 * Purpose: Merges metrics from a discarded segment into transaction
 *          metrics.
 *
 *          If no segment limit is set, a proper exclusive time is
 *          calculated for metrics of discarded segments. This also affects
 *          the parent segment of the discarded segment: to properly calculate
 *          exclusive time on the parent segment, the exclusive time data
 *          structure on the parent segment is initialized and this segment is
 *          added to it.
 *
 *          If no segment limit is set, the exclusive time of metrics of
 *          discarded segments is set to 0.
 *
 * Params:  1. A segment that is discarded. It is assumed that the
 *             segment has metrics.
 */
static void nr_segment_discard_merge_metrics(nr_segment_t* segment) {
  nrtime_t duration;
  nrtime_t exclusive_time;
  nr_segment_t* parent;
  size_t metric_count;
  size_t num_children;
  static nrtxn_t* warning_printed_for;

  if (nrunlikely(NULL == segment || NULL == segment->txn
                 || NULL == segment->parent)) {
    return;
  }

  if (nrunlikely(segment->stop_time < segment->start_time)) {
    return;
  }

  metric_count = nr_vector_size(segment->metrics);
  duration = nr_time_duration(segment->start_time, segment->stop_time);

  /*
   * In case a segment limit is set, calculating total time for metrics
   * of discarded segments is skipped.
   *
   * This has to be done to avoid uncontrollable memory usage: otherwise
   * the exclusive time structs on segments can grow uncontrollably
   * large.
   */
  if (segment->txn->options.max_segments > 1) {
    for (size_t i = 0; i < metric_count; i++) {
      nr_segment_metric_t* sm
          = (nr_segment_metric_t*)nr_vector_get(segment->metrics, i);

      nrm_add_ex(sm->scoped ? segment->txn->scoped_metrics
                            : segment->txn->unscoped_metrics,
                 sm->name, duration, 0);
    }

    if (warning_printed_for != segment->txn) {
      nrl_warning(
          NRL_SEGMENT,
          "skipping metric exclusive time calculation due to segment limit");
      warning_printed_for = segment->txn;
    }

    return;
  }

  /*
   * In case no segment limit is set, the correct exclusive time for all
   * metrics is calculated.
   */
  parent = segment->parent;
  num_children = nr_segment_children_size(&segment->children);

  /*
   * If this segment has children this has to be considered in the
   * exclusive time calculation for metrics.
   */
  if (num_children) {
    nr_exclusive_time_ensure(&segment->exclusive_time, num_children,
                             segment->start_time, segment->stop_time);

    for (size_t i = 0; i < num_children; i++) {
      nr_segment_t* child = nr_segment_children_get(&segment->children, i);

      if (child && child->async_context == segment->async_context) {
        nr_exclusive_time_add_child(segment->exclusive_time, child->start_time,
                                    child->stop_time);
      }
    }
  }

  /*
   * If no exclusive time data structure is initialized this means we are on
   * a leaf node of the "metrics tree". This means that this segment
   * has no children and no discarded children that had metrics. In that
   * case the exclusive time equals the duration.
   *
   * If that's not the case, we use the exclusive time data structure to
   * calculate exclusive time.
   */
  if (NULL == segment->exclusive_time) {
    exclusive_time = duration;
  } else {
    exclusive_time = nr_exclusive_time_calculate(segment->exclusive_time);
  }

  /*
   * If we're in the same execution context, this segment has to be
   * added to the exclusive time data structure of the parent. The
   * exclusive time on the parent is initialized if necessary.
   */
  if (segment->parent->async_context == segment->async_context) {
    nr_exclusive_time_ensure(&parent->exclusive_time,
                             nr_segment_children_size(&parent->children),
                             parent->start_time, parent->stop_time);

    nr_exclusive_time_add_child(parent->exclusive_time, segment->start_time,
                                segment->stop_time);
  }

  /*
   * Finally, metrics of this segment with the proper exclusive time and
   * duration are added to the transaction.
   */
  for (size_t i = 0; i < metric_count; i++) {
    nr_segment_metric_t* sm
        = (nr_segment_metric_t*)nr_vector_get(segment->metrics, i);

    nrm_add_ex(sm->scoped ? segment->txn->scoped_metrics
                          : segment->txn->unscoped_metrics,
               sm->name, duration, exclusive_time);
  }
}

nr_segment_t* nr_segment_start(nrtxn_t* txn,
                               nr_segment_t* parent,
                               const char* async_context) {
  nr_segment_t* new_segment;

  if (nrunlikely(NULL == txn)) {
    return NULL;
  }

  if (!txn->status.recording) {
    return NULL;
  }

  new_segment = nr_txn_allocate_segment(txn);
  if (nrunlikely(NULL == new_segment)) {
    return NULL;
  }

  nr_segment_init(new_segment, txn, parent, async_context);

  return new_segment;
}

bool nr_segment_init(nr_segment_t* segment,
                     nrtxn_t* txn,
                     nr_segment_t* parent,
                     const char* async_context) {
  if (nrunlikely(NULL == segment)) {
    return false;
  }

  segment->color = NR_SEGMENT_WHITE;
  segment->type = NR_SEGMENT_CUSTOM;
  segment->txn = txn;
  segment->error = NULL;

  /* A segment's time is expressed in terms of time relative to the
   * transaction. Determine the difference between the transaction's
   * start time and now. */
  segment->start_time = nr_txn_now_rel(txn);

  if (async_context) {
    segment->async_context = nr_string_add(txn->trace_strings, async_context);
  } else {
    segment->async_context = 0;
  }

  nr_segment_children_init(&segment->children);

  /* If an explicit parent has been passed in, parent this newly
   * started segment with the explicit parent. Make the newly-started
   * segment a sibling of its parent's (possibly) already-existing children. */
  if (parent) {
    segment->parent = parent;
    nr_segment_children_add(&parent->children, segment);
  } /* Otherwise, the parent of this new segment is the current segment on the
       transaction */
  else {
    nr_segment_t* current_segment
        = nr_txn_get_current_segment(txn, async_context);

    /* Special case: if the current segment is NULL and the async context is not
     * NULL, then this indicates that the new segment is the root of a new async
     * context. In that case, we'll parent it to the current segment on the main
     * context. (Users who want to have their new context be parented to another
     * async context will need to provide a parent explicitly.) */
    if (NULL == current_segment && NULL != async_context) {
      current_segment = nr_txn_get_current_segment(txn, NULL);
    }

    segment->parent = current_segment;

    if (NULL != current_segment) {
      nr_segment_children_add(&current_segment->children, segment);
    }
    nr_txn_set_current_segment(txn, segment);
  }

  return true;
}

static void nr_populate_datastore_spans(nr_span_event_t* span_event,
                                        const nr_segment_t* segment) {
  const char* port_path_or_id;
  const char* sql;
  const char* component;
  char* address;
  char* host;

  nr_span_event_set_category(span_event, NR_SPAN_DATASTORE);

  if (nrunlikely(NULL == segment || NULL == segment->typed_attributes)) {
    return;
  }

  component = segment->typed_attributes->datastore.component;
  nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_COMPONENT,
                              component);

  host = segment->typed_attributes->datastore.instance.host;
  nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_PEER_HOSTNAME,
                              host);

  port_path_or_id
      = segment->typed_attributes->datastore.instance.port_path_or_id;
  if (NULL == host) {
    /* When host is not set, it should be NULL when used as
     * NR_SPAN_DATASTORE_PEER_ADDRESS, however, when used in connection
     * with NR_SPAN_DATASTORE_PEER_ADDRESS it should be set to
     * "unknown".
     */
    host = "unknown";
  }
  if (NULL == port_path_or_id) {
    port_path_or_id = "unknown";
  }
  address = nr_formatf("%s:%s", host, port_path_or_id);
  nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_PEER_ADDRESS,
                              address);
  nr_free(address);

  nr_span_event_set_datastore(
      span_event, NR_SPAN_DATASTORE_DB_INSTANCE,
      segment->typed_attributes->datastore.instance.database_name);

  sql = segment->typed_attributes->datastore.sql;
  if (NULL == sql) {
    sql = segment->typed_attributes->datastore.sql_obfuscated;
  }
  nr_span_event_set_datastore(span_event, NR_SPAN_DATASTORE_DB_STATEMENT, sql);
}

static void nr_populate_http_spans(nr_span_event_t* span_event,
                                   const nr_segment_t* segment) {
  nr_span_event_set_category(span_event, NR_SPAN_HTTP);

  if (nrunlikely(NULL == segment || NULL == segment->typed_attributes)) {
    return;
  }

  nr_span_event_set_external(span_event, NR_SPAN_EXTERNAL_METHOD,
                             segment->typed_attributes->external.procedure);
  nr_span_event_set_external(span_event, NR_SPAN_EXTERNAL_URL,
                             segment->typed_attributes->external.uri);
  nr_span_event_set_external(span_event, NR_SPAN_EXTERNAL_COMPONENT,
                             segment->typed_attributes->external.library);
  nr_span_event_set_external_status(span_event,
                                    segment->typed_attributes->external.status);
}

static nr_status_t add_user_attribute_to_span_event(const char* key,
                                                    const nrobj_t* val,
                                                    void* ptr) {
  nr_span_event_t* event = NULL;
  nr_span_event_and_counter_t* event_and_counter = NULL;
  if (NULL == ptr || NULL == key || NULL == val) {
    return NR_FAILURE;
  }

  event_and_counter = (nr_span_event_and_counter_t*)ptr;
  event = event_and_counter->event;
  if (event_and_counter->counter > 0) {
    nr_span_event_set_attribute_user((nr_span_event_t*)event, key, val);

    event_and_counter->counter--;
  }

  return NR_SUCCESS;
}

static nr_status_t add_agent_attribute_to_span_event(const char* key,
                                                     const nrobj_t* val,
                                                     void* ptr) {
  const char* ignored_attributes[]
      = {"errorType", "errorMessage", "error.class", "error.message"};
  size_t i;

  for (i = 0; i < sizeof(ignored_attributes) / sizeof(ignored_attributes[0]);
       i++) {
    if (0 == nr_strcmp(key, ignored_attributes[i])) {
      return NR_SUCCESS;
    }
  }

  nr_span_event_set_attribute_agent((nr_span_event_t*)ptr, key, val);

  return NR_SUCCESS;
}

#define NR_APP_LOG_WARNING_SEGMENT_ID_FAILURE_BACKOFF_SECONDS 60

static void nr_segment_log_warning_segment_id_missing(void) {
  static unsigned n_occur = 0;
  static time_t last_warn = (time_t)(0);
  time_t now = time(0);

  n_occur++;

  if ((now - last_warn)
      > NR_APP_LOG_WARNING_SEGMENT_ID_FAILURE_BACKOFF_SECONDS) {
    last_warn = now;
    nrl_warning(
        NRL_SEGMENT,
        "cannot create a span event when a segment ID cannot be "
        "generated; is distributed tracing enabled?  Occurred %u times.",
        n_occur);
    n_occur = 0;
  }
}

nr_span_event_t* nr_segment_to_span_event(nr_segment_t* segment) {
  nr_span_event_t* event;
  char* trace_id;
  nrobj_t* user_attributes;
  nrobj_t* agent_attributes;
  nr_span_event_and_counter_t event_and_counter;

  if (NULL == segment) {
    return NULL;
  }

  if (0 == segment->stop_time) {
    nrl_warning(NRL_SEGMENT,
                "cannot create a span event from an active segment");
    return NULL;
  }

  if (nrunlikely(segment->start_time > segment->stop_time)) {
    nrl_warning(NRL_SEGMENT,
                "cannot create a span event when the stop time is before the "
                "start time: " NR_TIME_FMT " > " NR_TIME_FMT,
                segment->start_time, segment->stop_time);
    return NULL;
  }

  if (NULL == nr_segment_ensure_id(segment, segment->txn)) {
    nr_segment_log_warning_segment_id_missing();
    return NULL;
  }

  trace_id = nr_txn_get_current_trace_id(segment->txn);
  event = nr_span_event_create();
  nr_span_event_set_guid(event, segment->id);
  nr_span_event_set_trace_id(event, trace_id);
  nr_span_event_set_transaction_id(event, nr_txn_get_guid(segment->txn));
  nr_span_event_set_name(
      event, nr_string_get(segment->txn->trace_strings, segment->name));
  nr_span_event_set_timestamp(
      event, nr_txn_time_rel_to_abs(segment->txn, segment->start_time));
  nr_span_event_set_duration(
      event, nr_time_duration(segment->start_time, segment->stop_time));
  nr_span_event_set_priority(event, nr_distributed_trace_get_priority(
                                        segment->txn->distributed_trace));
  nr_span_event_set_sampled(
      event, nr_distributed_trace_is_sampled(segment->txn->distributed_trace));

  if (segment->parent) {
    nr_segment_ensure_id(segment->parent, segment->txn);
    nr_span_event_set_parent_id(event, segment->parent->id);
    nr_span_event_set_entry_point(event, false);
  } else {
    nr_span_event_set_entry_point(event, true);
    nr_span_event_set_tracing_vendors(
        event, nr_distributed_trace_inbound_get_tracing_vendors(
                   segment->txn->distributed_trace));
    nr_span_event_set_trusted_parent_id(
        event, nr_distributed_trace_inbound_get_trusted_parent_id(
                   segment->txn->distributed_trace));
    nr_span_event_set_parent_id(event, 
        nr_distributed_trace_inbound_get_guid(segment->txn->distributed_trace));

    nr_span_event_set_transaction_name(event, segment->txn->name);

    // Add transaction parent attributes to the service entry span
    if (segment->txn->type & NR_TXN_TYPE_DT_INBOUND) {
      nr_span_event_set_parent_attribute(event, NR_SPAN_PARENT_TYPE,
                                         nr_distributed_trace_inbound_get_type(
                                             segment->txn->distributed_trace));
      nr_span_event_set_parent_attribute(
          event, NR_SPAN_PARENT_APP,
          nr_distributed_trace_inbound_get_app_id(
              segment->txn->distributed_trace));
      nr_span_event_set_parent_attribute(
          event, NR_SPAN_PARENT_ACCOUNT,
          nr_distributed_trace_inbound_get_account_id(
              segment->txn->distributed_trace));
      nr_span_event_set_parent_attribute(
          event, NR_SPAN_PARENT_TRANSPORT_TYPE,
          nr_distributed_trace_inbound_get_transport_type(
              segment->txn->distributed_trace));
      if (nr_distributed_trace_inbound_has_timestamp(
              segment->txn->distributed_trace)) {
        nr_span_event_set_parent_transport_duration(
            event, nr_distributed_trace_inbound_get_timestamp_delta(
                       segment->txn->distributed_trace,
                       nr_txn_start_time(segment->txn)));
      }
    }

    agent_attributes = nr_attributes_agent_to_obj(
        segment->txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT);
    nro_iteratehash(agent_attributes, add_agent_attribute_to_span_event, event);
    nro_delete(agent_attributes);
  }

  if (segment->error) {
    nr_span_event_set_error_message(event, segment->error->error_message);
    nr_span_event_set_error_class(event, segment->error->error_class);
  }

  switch (segment->type) {
    case NR_SEGMENT_DATASTORE:
      nr_populate_datastore_spans(event, segment);
      break;

    case NR_SEGMENT_EXTERNAL:
      nr_populate_http_spans(event, segment);
      break;

    case NR_SEGMENT_CUSTOM:
      nr_span_event_set_category(event, NR_SPAN_GENERIC);
      break;

    default:
      nrl_warning(NRL_AGENT,
                  "unexpected segment type when creating span event: %d",
                  (int)segment->type);
      nr_span_event_set_category(event, NR_SPAN_GENERIC);
  }

  event_and_counter.event = event;
  event_and_counter.counter = NR_ATTRIBUTE_USER_LIMIT;

  if (segment->attributes) {
    user_attributes = nr_attributes_user_to_obj(segment->attributes,
                                                NR_ATTRIBUTE_DESTINATION_SPAN);

    nro_iteratehash(user_attributes, add_user_attribute_to_span_event,
                    &event_and_counter);

    nro_delete(user_attributes);
    /*
     * Add segment agent attributes to span
     */
    agent_attributes = nr_attributes_agent_to_obj(
        segment->attributes, NR_ATTRIBUTE_DESTINATION_SPAN);
    nro_iteratehash(agent_attributes, add_agent_attribute_to_span_event, event);
    nro_delete(agent_attributes);
  }
  if (segment->attributes_txn_event) {
    user_attributes = nr_attributes_user_to_obj(segment->attributes_txn_event,
                                                NR_ATTRIBUTE_DESTINATION_SPAN);
    nro_iteratehash(user_attributes, add_user_attribute_to_span_event,
                    &event_and_counter);

    nro_delete(user_attributes);
  }

  nr_free(trace_id);
  return event;
}

bool nr_segment_set_custom(nr_segment_t* segment) {
  if (NULL == segment) {
    return false;
  }

  if (NR_SEGMENT_CUSTOM == segment->type) {
    return true;
  }

  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
  segment->type = NR_SEGMENT_CUSTOM;

  return true;
}

bool nr_segment_set_datastore(nr_segment_t* segment,
                              const nr_segment_datastore_t* datastore) {
  if (nrunlikely((NULL == segment || NULL == datastore))) {
    return false;
  }

  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
  segment->type = NR_SEGMENT_DATASTORE;

  segment->typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));

  // clang-format off
  // Initialize the fields of the datastore attributes, one field per line.
  segment->typed_attributes->datastore = (nr_segment_datastore_t){
      .component = datastore->component ? nr_strdup(datastore->component) : NULL,
      .sql = datastore->sql ? nr_strdup(datastore->sql) : NULL,
      .sql_obfuscated = datastore->sql_obfuscated ? nr_strdup(datastore->sql_obfuscated) : NULL,
      .input_query_json = datastore->input_query_json ? nr_strdup(datastore->input_query_json) : NULL,
      .backtrace_json = datastore->backtrace_json ? nr_strdup(datastore->backtrace_json) : NULL,
      .explain_plan_json = datastore->explain_plan_json ? nr_strdup(datastore->explain_plan_json) : NULL,
  };

  segment->typed_attributes->datastore.instance = (nr_datastore_instance_t){
      .host = datastore->instance.host ? nr_strdup(datastore->instance.host) : NULL,
      .port_path_or_id = datastore->instance.port_path_or_id ? nr_strdup(datastore->instance.port_path_or_id) : NULL,
      .database_name = datastore->instance.database_name ? nr_strdup(datastore->instance.database_name): NULL,
  };
  // clang-format on

  return true;
}

bool nr_segment_set_external(nr_segment_t* segment,
                             const nr_segment_external_t* external) {
  if (nrunlikely((NULL == segment) || (NULL == external))) {
    return false;
  }

  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
  segment->type = NR_SEGMENT_EXTERNAL;
  segment->typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));

  // clang-format off
  // Initialize the fields of the external attributes, one field per line.
  segment->typed_attributes->external = (nr_segment_external_t){
      .transaction_guid = external->transaction_guid ? nr_strdup(external->transaction_guid) : NULL,
      .uri = external->uri ? nr_strdup(external->uri) : NULL,
      .library = external->library ? nr_strdup(external->library) : NULL,
      .procedure = external->procedure ? nr_strdup(external->procedure) : NULL,
      .status = external->status,
  };
  // clang-format on

  return true;
}

bool nr_segment_add_child(nr_segment_t* parent, nr_segment_t* child) {
  if (nrunlikely((NULL == parent) || (NULL == child))) {
    return false;
  }

  nr_segment_set_parent(child, parent);

  return true;
}

static void nr_segment_metric_destroy_wrapper(void* sm,
                                              void* userdata NRUNUSED) {
  nr_segment_metric_destroy_fields((nr_segment_metric_t*)sm);
  nr_free(sm);
}

bool nr_segment_add_metric(nr_segment_t* segment,
                           const char* name,
                           bool scoped) {
  nr_segment_metric_t* sm;

  if (nrunlikely(NULL == segment || NULL == name)) {
    return false;
  }

  if (NULL == segment->metrics) {
    /* We'll use 4 as the default vector size here because that's the most
     * metrics we should see from an automatically instrumented segment: legacy
     * CAT will create scoped and unscoped rollup and ExternalTransaction
     * metrics. */
    segment->metrics
        = nr_vector_create(4, nr_segment_metric_destroy_wrapper, NULL);
  }

  sm = nr_malloc(sizeof(nr_segment_metric_t));
  sm->name = nr_strdup(name);
  sm->scoped = scoped;

  return nr_vector_push_back(segment->metrics, sm);
}

bool nr_segment_set_name(nr_segment_t* segment, const char* name) {
  if ((NULL == segment) || (NULL == name)) {
    return false;
  }

  segment->name = nr_string_add(segment->txn->trace_strings, name);

  return true;
}

bool nr_segment_set_parent(nr_segment_t* segment, nr_segment_t* parent) {
  nr_segment_t* ancestor = NULL;

  if (NULL == segment) {
    return false;
  }

  if (NULL != parent && segment->txn != parent->txn) {
    return false;
  }

  if (segment->parent == parent) {
    return true;
  }

  /*
   * Check if we are creating a cycle. If the to-be child segment is a child
   * of the to-be parent segment then we are creating a cycle. We should not
   * continue.
   */
  ancestor = parent;
  while (NULL != ancestor) {
    if (ancestor == segment) {
      nrl_warning(NRL_API,
                  "Unsuccessful call to newrelic_set_segment_parent(). Cannot "
                  "set parent because it would introduce a cycle into the "
                  "agent's call stack representation.");
      return false;
    }
    ancestor = ancestor->parent;
  }

  if (segment->parent) {
    nr_segment_children_remove(&segment->parent->children, segment);
  }

  nr_segment_children_add(&parent->children, segment);
  segment->parent = parent;

  return true;
}

bool nr_segment_set_timing(nr_segment_t* segment,
                           nrtime_t start,
                           nrtime_t duration) {
  if (NULL == segment) {
    return false;
  }

  segment->start_time = start;
  segment->stop_time = start + duration;

  return true;
}

bool nr_segment_end(nr_segment_t** segment_ptr) {
  nrtxn_t* txn = NULL;
  nr_segment_t* segment;

  if (nrunlikely(NULL == segment_ptr || NULL == *segment_ptr
                 || NULL == (*segment_ptr)->txn)) {
    nrl_verbosedebug(NRL_API, "nr_segment_end: cannot end null segment");
    return false;
  }

  segment = *segment_ptr;
  txn = segment->txn;

  if (0 == segment->stop_time) {
    /* A segment's time is expressed in terms of time relative to the
     * transaction. Determine the difference between the transaction's start
     * time and now. */
    segment->stop_time
        = nr_time_duration(nr_txn_start_time(txn), nr_get_time());
  }

  txn->segment_count += 1;
  nr_txn_retire_current_segment(txn, segment);

  nr_minmax_heap_insert(txn->segment_heap, segment);

  (*segment_ptr) = NULL;

  return true;
}

/*
 * Purpose : Given a segment color, return the other color.
 *
 * Params  : 1. The color to toggle.
 *
 * Returns : The toggled color.
 */
static nr_segment_color_t nr_segment_toggle_color(nr_segment_color_t color) {
  if (NR_SEGMENT_WHITE == color) {
    return NR_SEGMENT_GREY;
  } else {
    return NR_SEGMENT_WHITE;
  }
}

/*
 * Purpose : The callback registered by nr_segment_destroy_children_callback()
 *           to finish destroying the segment and (if necessary) its child
 *           structures.
 */
static void nr_segment_destroy_children_post_callback(nr_segment_t* segment,
                                                      void* userdata NRUNUSED) {
  /* Free the fields within the segment */
  nr_segment_destroy_fields(segment);
  nr_segment_children_deinit(&segment->children);
}

/*
 * Purpose : The callback necessary to iterate over a
 * tree of segments and free them and all their children.
 */
static nr_segment_iter_return_t nr_segment_destroy_children_callback(
    nr_segment_t* segment NRUNUSED,
    void* userdata NRUNUSED) {
  return ((nr_segment_iter_return_t){
      .post_callback = nr_segment_destroy_children_post_callback});
}

/*
 * Purpose : Iterate over the segments in a tree of segments.
 *
 * Params  : 1. A pointer to the root.
 *           2. The color of a segment not yet traversed.
 *           3. The color of a segment already traversed.
 *           4. The iterator function to be invoked for each segment
 *           5. Optional userdata for the iterator.
 *
 * Notes   : This iterator is hardened against infinite regress. Even
 *           when there are ill-formed cycles in the tree, the
 *           iteration will terminate because it colors the segments
 *           as it traverses them.
 */
static void nr_segment_iterate_helper(nr_segment_t* root,
                                      nr_segment_color_t reset_color,
                                      nr_segment_color_t set_color,
                                      nr_segment_iter_t callback,
                                      void* userdata) {
  if (NULL == root) {
    return;
  } else {
    // Color the segments as the tree is traversed to prevent infinite regress.
    if (reset_color == root->color) {
      nr_segment_iter_return_t cb_return;
      size_t i;
      size_t n_children = nr_segment_children_size(&root->children);

      root->color = set_color;

      // Invoke the pre-traversal callback.
      cb_return = (callback)(root, userdata);

      // Iterate the children.
      for (i = 0; i < n_children; i++) {
        nr_segment_iterate_helper(nr_segment_children_get(&root->children, i),
                                  reset_color, set_color, callback, userdata);
      }

      // If a post-traversal callback was registered, invoke it.
      if (cb_return.post_callback) {
        (cb_return.post_callback)(root, cb_return.userdata);
      }
    }
  }
}

void nr_segment_iterate(nr_segment_t* root,
                        nr_segment_iter_t callback,
                        void* userdata) {
  if (nrunlikely(NULL == callback)) {
    return;
  }

  if (nrunlikely(NULL == root)) {
    return;
  }
  /* What is the color of the root?  Assume the whole tree is that color.
   * The tree of segments is never partially traversed, so this assumption
   * is well-founded.
   *
   * That said, if there were a case in which the tree had been partially
   * traversed, and is traversed again, the worse case scenario would be that a
   * subset of the tree is not traversed. */
  nr_segment_iterate_helper(root, root->color,
                            nr_segment_toggle_color(root->color), callback,
                            userdata);
}

void nr_segment_destroy_tree(nr_segment_t* root) {
  if (NULL == root) {
    return;
  }

  nr_segment_iterate(
      root, (nr_segment_iter_t)nr_segment_destroy_children_callback, NULL);
}

bool nr_segment_discard(nr_segment_t** segment_ptr) {
  nr_segment_t* segment = NULL;
  nrtxn_t* txn = NULL;

  if (NULL == segment_ptr || NULL == *segment_ptr
      || NULL == (*segment_ptr)->txn) {
    return false;
  }

  segment = *segment_ptr;
  txn = segment->txn;

  /* Don't discard the root node. */
  if (nrunlikely(NULL == segment->parent)) {
    nrl_warning(NRL_API, "Illegal action: Tried to discard ROOT segment");
    return false;
  }

  /*
   * Remove the segment from the active stack before deinitializing it.
   */
  nr_txn_retire_current_segment(segment->txn, segment);

  /*
   * Merge metrics into the transaction's metric tables.
   */
  if (nr_vector_size(segment->metrics)) {
    nr_segment_discard_merge_metrics(segment);
  }

  /* Unhook the segment from its parent. */
  if (!nr_segment_children_remove(&segment->parent->children, segment)) {
    return false;
  }

  /* Reparent all children. */
  nr_segment_children_reparent(&segment->children, segment->parent);

  nr_segment_children_deinit(&segment->children);

  /* Free memory. */
  nr_segment_destroy_fields(segment);
  nr_slab_release(txn->segment_slab, segment);
  (*segment_ptr) = NULL;

  return true;
}

/*
 * Safety check for comparator functions
 *
 * This avoids NULL checks in each comparator and ensures that NULL
 * elements are consistently considered as smaller.
 */
#define COMPARATOR_NULL_CHECK(__elem1, __elem2)             \
  if (nrunlikely(NULL == (__elem1) || NULL == (__elem2))) { \
    if ((__elem1) < (__elem2)) {                            \
      return -1;                                            \
    } else if ((__elem1) > (__elem2)) {                     \
      return 1;                                             \
    }                                                       \
    return 0;                                               \
  }

static int nr_segment_duration_comparator(const nr_segment_t* a,
                                          const nr_segment_t* b) {
  nrtime_t duration_a = a->stop_time - a->start_time;
  nrtime_t duration_b = b->stop_time - b->start_time;

  if (duration_a < duration_b) {
    return -1;
  } else if (duration_a > duration_b) {
    return 1;
  }
  return 0;
}

int nr_segment_wrapped_duration_comparator(const void* a,
                                           const void* b,
                                           void* userdata NRUNUSED) {
  COMPARATOR_NULL_CHECK(a, b);

  return nr_segment_duration_comparator((const nr_segment_t*)a,
                                        (const nr_segment_t*)b);
}

static int nr_segment_span_priority_comparator(const nr_segment_t* a,
                                               const nr_segment_t* b) {
  if (a->priority > b->priority) {
    return 1;
  } else if (a->priority < b->priority) {
    return -1;
  } else {
    return nr_segment_duration_comparator(a, b);
  }
}

int nr_segment_wrapped_span_priority_comparator(const void* a,
                                                const void* b,
                                                void* userdata NRUNUSED) {
  COMPARATOR_NULL_CHECK(a, b);

  return nr_segment_span_priority_comparator((const nr_segment_t*)a,
                                             (const nr_segment_t*)b);
}

nr_minmax_heap_t* nr_segment_heap_create(ssize_t bound,
                                         nr_minmax_heap_cmp_t comparator) {
  return nr_minmax_heap_create(bound, comparator, NULL, NULL, NULL);
}

static void nr_segment_stoh_post_iterator_callback(
    nr_segment_t* segment,
    nr_segment_tree_to_heap_metadata_t* metadata) {
  nrtime_t exclusive_time;
  size_t i;
  size_t metric_count;

  if (nrunlikely(NULL == segment || NULL == metadata)) {
    return;
  }

  // Calculate the exclusive time.
  exclusive_time = nr_exclusive_time_calculate(segment->exclusive_time);

  // Update the transaction total time.
  metadata->total_time += exclusive_time;

  // Merge any segment metrics with the transaction metric tables.
  metric_count = nr_vector_size(segment->metrics);
  for (i = 0; i < metric_count; i++) {
    nr_segment_metric_t* sm
        = (nr_segment_metric_t*)nr_vector_get(segment->metrics, i);

    nrm_add_ex(sm->scoped ? segment->txn->scoped_metrics
                          : segment->txn->unscoped_metrics,
               sm->name,
               nr_time_duration(segment->start_time, segment->stop_time),
               exclusive_time);
  }

  /*
   * Don't discard the exclusive time structure for the root segment, as
   * it is needed when creating transaction metrics.
   */
  if (segment->parent) {
    nr_exclusive_time_destroy(&segment->exclusive_time);
  }
}

/*
 * Purpose : Place an nr_segment_t pointer into a nr_minmax_heap,
 *             or "segments to heap".
 *
 * Params  : 1. The segment pointer to place into the heap.
 *           2. A void* pointer to be recast as the pointer to the heap.
 *
 * Note    : This is the callback function supplied to nr_segment_iterate(),
 *           used for iterating over a tree of segments and placing each
 *           segment into the heap.
 */
static nr_segment_iter_return_t nr_segment_stoh_iterator_callback(
    nr_segment_t* segment,
    void* userdata) {
  nr_minmax_heap_t* trace_heap = NULL;
  nr_minmax_heap_t* span_heap = NULL;
  nr_segment_tree_to_heap_metadata_t* metadata
      = (nr_segment_tree_to_heap_metadata_t*)userdata;

  if (nrunlikely(NULL == segment) || nrunlikely(NULL == userdata)) {
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  /* Set up the exclusive time so that children can adjust it as necessary. */
  nr_exclusive_time_ensure(&segment->exclusive_time,
                           nr_segment_children_size(&segment->children),
                           segment->start_time, segment->stop_time);

  /* Adjust the parent's exclusive time. */
  if (segment->parent
      && segment->parent->async_context == segment->async_context) {
    nr_exclusive_time_add_child(segment->parent->exclusive_time,
                                segment->start_time, segment->stop_time);
  }

  /*
   * Adjust the main context exclusive time if necessary.
   *
   * This supports the discount_main_context_blocking transaction option: if
   * that option is enabled, then the metadata will have a non-NULL main context
   * exclusive time pointer. If the current segment is asynchronous, then we
   * need to add the segment to the main context exclusive time structure so the
   * blocking time can be calculated once the first pass is complete.
   */
  if (segment->async_context && metadata->main_context) {
    nr_exclusive_time_add_child(metadata->main_context, segment->start_time,
                                segment->stop_time);
  }

  trace_heap = metadata->trace_heap;
  span_heap = metadata->span_heap;

  if (NULL != trace_heap) {
    nr_minmax_heap_insert(trace_heap, segment);
  }

  if (NULL != span_heap) {
    nr_minmax_heap_insert(span_heap, segment);
  }

  // clang-format off
  return ((nr_segment_iter_return_t){
    .post_callback
      = (nr_segment_post_iter_t)nr_segment_stoh_post_iterator_callback,
    .userdata = metadata,
  });
  // clang-format on
}

void nr_segment_tree_to_heap(nr_segment_t* root,
                             nr_segment_tree_to_heap_metadata_t* metadata) {
  if (NULL == root || NULL == metadata) {
    return;
  }
  /* Convert the tree to two minmax heap.  The bound, or
   * size, of the heaps, and the comparison functions installed
   * by the nr_segment_heap_create() calls will assure that the
   * segments in the heaps are of highest priority. */
  nr_segment_iterate(root, (nr_segment_iter_t)nr_segment_stoh_iterator_callback,
                     metadata);
}

/*
 * Purpose : Place an nr_segment_t pointer in a heap into a nr_set_t,
 *             or "heap to set".
 *
 * Params  : 1. The segment pointer in the heap.
 *           2. A void* pointer to be recast as the pointer to the set.
 *
 * Note    : This is the callback function supplied to nr_minmax_heap_iterate
 *           used for iterating over a heap of segments and placing each
 *           segment into a set.
 */
static bool nr_segment_htos_iterator_callback(void* value, void* userdata) {
  if (nrlikely(value && userdata)) {
    nr_set_t* set = (nr_set_t*)userdata;
    nr_set_insert(set, value);
  }

  return true;
}

void nr_segment_heap_to_set(nr_minmax_heap_t* heap, nr_set_t* set) {
  if (NULL == heap || NULL == set) {
    return;
  }

  /* Convert the heap to a set */
  nr_minmax_heap_iterate(
      heap, (nr_minmax_heap_iter_t)nr_segment_htos_iterator_callback,
      (void*)set);

  return;
}

char* nr_segment_ensure_id(nr_segment_t* segment, const nrtxn_t* txn) {
  if (nrunlikely(NULL == segment || NULL == txn)) {
    return NULL;
  }

  // Create a segment id if it doesn't exist.
  if ((NULL == segment->id) && (nr_txn_should_create_span_events(txn))) {
    segment->id = nr_guid_create(txn->rnd);
  }

  return segment->id;
}

void nr_segment_set_priority_flag(nr_segment_t* segment, int flag) {
  if (NULL == segment) {
    return;
  }

  segment->priority |= flag;
}

int nr_segment_get_priority_flag(nr_segment_t* segment) {
  if (NULL == segment) {
    return 0;
  }

  return segment->priority;
}

void nr_segment_record_exception(nr_segment_t* segment,
                                 const char* error_message,
                                 const char* error_class) {
  if ((NULL == segment) || (NULL == segment->txn)
      || (NULL == error_message && NULL == error_class)) {
    return;
  }

  if (0 == segment->txn->options.err_enabled
      || (0 == segment->txn->status.recording)) {
    return;
  }

  if (segment->txn->high_security) {
    error_message = NR_TXN_HIGH_SECURITY_ERROR_MESSAGE;
  }

  if (0 == segment->txn->options.allow_raw_exception_messages) {
    error_message = NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE;
  }

  nr_segment_set_error(segment, error_message, error_class);
}

void nr_segment_set_error(nr_segment_t* segment,
                          const char* error_message,
                          const char* error_class) {
  if ((NULL == segment) || (NULL == error_message && NULL == error_class)) {
    return;
  }
  nr_segment_set_error_with_additional_params(segment, error_message, error_class, NULL, 0,
                              NULL, 0);
}

void nr_segment_set_error_with_additional_params(nr_segment_t* segment,
                                                 const char* error_message,
                                                 const char* error_class,
                                                 const char* error_file,
                                                 int error_line,
                                                 char* error_context,
                                                 int error_no) {
  if (NULL == segment || NULL == error_class) {
    return;
  }
  
  if (NULL == segment->error) {
    segment->error = nr_zalloc(sizeof(nr_segment_error_t));
  }

  nr_free(segment->error->error_message);
  nr_free(segment->error->error_class);
  nr_free(segment->error->error_file);
  nr_free(segment->error->error_context);

  segment->error->error_class = error_class ? nr_strdup(error_class) : NULL;
  segment->error->error_no = error_no;
  segment->error->error_line = error_line;
  if (NULL != error_message) {
    segment->error->error_message = error_message ? nr_strdup(error_message) : NULL;
  }
  if (NULL != error_file) {
    segment->error->error_file = error_file ? nr_strdup(error_file) : NULL;
  }
  if (NULL != error_context) {
    segment->error->error_context = error_context ? nr_strdup(error_context) : NULL;
  }
}

bool nr_segment_attributes_user_add(nr_segment_t* segment,
                                    uint32_t destination,
                                    const char* name,
                                    const nrobj_t* value) {
  nr_status_t status;

  if (NULL == segment || NULL == segment->txn || NULL == name
      || NULL == value) {
    return false;
  }

  if (NULL == segment->attributes) {
    segment->attributes = nr_attributes_create(segment->txn->attribute_config);
  }

  status
      = nr_attributes_user_add(segment->attributes, destination, name, value);
  nr_attributes_remove_attribute(segment->attributes_txn_event, name, 1);

  nr_segment_set_priority_flag(segment, NR_SEGMENT_PRIORITY_ATTR);

  return (NR_SUCCESS == status);
}

bool nr_segment_attributes_user_txn_event_add(nr_segment_t* segment,
                                              uint32_t destination,
                                              const char* name,
                                              const nrobj_t* value) {
  nr_status_t status;

  if (NULL == segment || NULL == segment->txn || NULL == name
      || NULL == value) {
    return false;
  }

  if (NULL == segment->attributes_txn_event) {
    segment->attributes_txn_event
        = nr_attributes_create(segment->txn->attribute_config);
  }

  if (nr_attributes_user_exists(segment->attributes, name)) {
    return false;
  }

  status = nr_attributes_user_add(segment->attributes_txn_event, destination,
                                  name, value);
  nr_segment_set_priority_flag(segment, NR_SEGMENT_PRIORITY_ATTR);

  return (NR_SUCCESS == status);
}

ssize_t nr_segment_get_child_ix(const nr_segment_t* segment) {
  if (NULL == segment) {
    return -1;
  }
  return segment->child_ix;
}

void nr_segment_set_child_ix(nr_segment_t* segment, size_t ix) {
  if (NULL != segment) {
    segment->child_ix = ix;
  }
}
