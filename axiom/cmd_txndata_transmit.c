/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains the agent's view of the transaction data command:
 * the payload of data that is sent to the daemon at the end of every
 * transaction.
 */
#include "nr_axiom.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_agent.h"
#include "nr_analytics_events.h"
#include "nr_app.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_distributed_trace.h"
#include "nr_limits.h"
#include "nr_php_packages.h"
#include "nr_slowsqls.h"
#include "nr_span_event.h"
#include "nr_synthetics.h"
#include "nr_txn.h"
#include "util_apdex.h"
#include "util_buffer.h"
#include "util_errno.h"
#include "util_flatbuffers.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_network.h"
#include "util_strings.h"
#include "util_syscalls.h"

char* nr_txndata_error_to_json(const nrtxn_t* txn) {
  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;
  char* json;

  if (0 == txn->error) {
    return NULL;
  }

  agent_attributes = nr_attributes_agent_to_obj(txn->attributes,
                                                NR_ATTRIBUTE_DESTINATION_ERROR);
  user_attributes = nr_attributes_user_to_obj(txn->attributes,
                                              NR_ATTRIBUTE_DESTINATION_ERROR);

  json = nr_error_to_daemon_json(txn->error, txn->name, agent_attributes,
                                 user_attributes, txn->intrinsics,
                                 txn->request_uri);

  nro_delete(agent_attributes);
  nro_delete(user_attributes);

  return json;
}

static uint32_t nr_txndata_prepend_custom_events(nr_flatbuffer_t* fb,
                                                 const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t events;
  int i;
  int event_count;

  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);

  event_count = nr_analytics_events_number_saved(txn->custom_events);
  if (0 == event_count) {
    return 0;
  }

  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));
  offset = &offsets[0];

  /*
   * Iterate in reverse order to satisfy the integration tests, which should
   * probably be changed to compare custom events in an order agnostic way.
   */
  for (i = event_count - 1; i >= 0; i--, offset++) {
    const char* json;
    uint32_t data;

    json = nr_analytics_events_get_event_json(txn->custom_events, i);
    data = nr_flatbuffers_prepend_string(fb, json);

    nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
    *offset = nr_flatbuffers_object_end(fb);
  }

  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  for (i = 0; i < event_count; i++) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  events = nr_flatbuffers_vector_end(fb, event_count);

  nr_free(offsets);
  return events;
}

static uint32_t nr_txndata_prepend_log_events(nr_flatbuffer_t* fb,
                                              const nrtxn_t* txn,
                                              size_t log_event_limit) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t events;
  size_t i;
  size_t event_count;
  nr_vector_t* events_vec;
  nrbuf_t* buf;

  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);

  event_count = nr_log_events_number_saved(txn->log_events);
  if (0 == event_count) {
    return 0;
  } else if (event_count > log_event_limit) {
    event_count = log_event_limit;
  }

  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));
  offset = &offsets[0];

  /* convert log events from heap into a vector */
  events_vec = nr_vector_create(event_count, NULL, NULL);
  nr_log_events_to_vector(txn->log_events, events_vec);

  /* Using a buffer here means we can write the encoded log events into it,
   * saving a couple of allocations (including at least one full string
   * duplication) per log event. */
  buf = nr_buffer_create(0, 0);

  for (i = 0; i < event_count; i++, offset++) {
    void* event;
    uint32_t data;
    bool found;

    nr_buffer_reset(buf);
    found = nr_vector_get_element(events_vec, i, &event);
    if (!found) {
      /* There's really no scenario this should happen, so we won't try to do
       * anything clever in terms of skipping the event and patching up the
       * offsets. Let's just assume it's going to be bad and move on. */
      nrl_error(NRL_TXN,
                "unable to retrieve log event at index %zu; buffer will be "
                "malformed",
                i);
      continue;
    }

    if (!nr_log_event_to_json_buffer(event, buf)) {
      /* There's really no scenario this should happen, so we won't try to do
       * anything clever in terms of skipping the event and patching up the
       * offsets. Let's just assume it's going to be bad and move on. */
      nrl_error(NRL_TXN,
                "unable to encode log event at index %zu; buffer will be "
                "malformed",
                i);
      continue;
    }

    data = nr_flatbuffers_prepend_bytes(fb, nr_buffer_cptr(buf),
                                        nr_buffer_len(buf));

    nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
    *offset = nr_flatbuffers_object_end(fb);
  }

  nr_buffer_destroy(&buf);

  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  for (i = 0; i < event_count; i++) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  events = nr_flatbuffers_vector_end(fb, event_count);

  nr_vector_destroy(&events_vec);
  nr_free(offsets);
  return events;
}

uint32_t nr_txndata_prepend_span_events(nr_flatbuffer_t* fb,
                                        nr_vector_t* span_events,
                                        size_t span_event_limit) {
  size_t i;
  size_t event_count = nr_vector_size(span_events);
  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);
  uint32_t data;
  uint32_t* offsets;
  nrbuf_t* buf;

  if (0 == event_count) {
    return 0;
  } else if (event_count > span_event_limit) {
    event_count = span_event_limit;
  }

  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));

  /* Using a buffer here means we can write the encoded span events into it,
   * saving a couple of allocations (including at least one full string
   * duplication) per span event. */
  buf = nr_buffer_create(0, 0);

  for (i = 0; i < event_count; i++) {
    const nr_span_event_t* span
        = (const nr_span_event_t*)nr_vector_get(span_events, i);

    nr_buffer_reset(buf);
    if (!nr_span_event_to_json_buffer(span, buf)) {
      /* There's really no scenario this should happen, so we won't try to do
       * anything clever in terms of skipping the event and patching up the
       * offsets. Let's just assume it's going to be bad and move on. */
      nrl_error(NRL_TXN,
                "unable to encode span event %p at index %zu; buffer will be "
                "malformed",
                span, i);
      continue;
    }

    data = nr_flatbuffers_prepend_bytes(fb, nr_buffer_cptr(buf),
                                        nr_buffer_len(buf));

    nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
    offsets[i] = nr_flatbuffers_object_end(fb);
  }

  nr_buffer_destroy(&buf);

  /*
   * Adding all offsets to the flatbuffer vector.
   */
  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  i = event_count;
  do {
    i -= 1;
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  } while (i > 0);
  data = nr_flatbuffers_vector_end(fb, event_count);

  nr_free(offsets);

  return data;
}

static uint32_t nr_txndata_prepend_errors(nr_flatbuffer_t* fb,
                                          const nrtxn_t* txn) {
  char* json;
  int32_t priority;
  uint32_t data;
  uint32_t error;

  const size_t error_count = 1;
  const size_t error_size = sizeof(uint32_t);
  const size_t error_align = sizeof(uint32_t);

  json = nr_txndata_error_to_json(txn);
  if (NULL == json) {
    return 0;
  }

  data = nr_flatbuffers_prepend_string(fb, json);
  nr_free(json);

  priority = nr_error_priority(txn->error);

  nr_flatbuffers_object_begin(fb, ERROR_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, ERROR_FIELD_DATA, data, 0);
  nr_flatbuffers_object_prepend_i32(fb, ERROR_FIELD_PRIORITY, priority, 0);
  error = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_vector_begin(fb, error_size, error_count, error_align);
  nr_flatbuffers_prepend_uoffset(fb, error);
  return nr_flatbuffers_vector_end(fb, error_count);
}

static double nrtime_to_double(nrtime_t x) {
  return (double)x;
}

static uint32_t nr_txndata_prepend_metric_data(nr_flatbuffer_t* fb,
                                               const nrmetric_t* metric,
                                               int scoped) {
  nr_flatbuffers_prep(fb, 8, 56);
  nr_flatbuffers_pad(fb, 6);
  nr_flatbuffers_prepend_bool(fb, nrm_is_forced(metric));
  nr_flatbuffers_prepend_bool(fb, scoped);

  if (nrm_is_apdex(metric)) {
    nr_flatbuffers_prepend_f64(fb, 0);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_max(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_min(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_failing(metric)));
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_tolerating(metric)));
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_satisfying(metric)));
  } else {
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_sumsquares(metric))
                                       / NR_TIME_DIVISOR_D_SQUARE);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_max(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_min(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_exclusive(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(
        fb, nrtime_to_double(nrm_total(metric)) / NR_TIME_DIVISOR_D);
    nr_flatbuffers_prepend_f64(fb, nrtime_to_double(nrm_count(metric)));
  }

  return nr_flatbuffers_len(fb);
}

static uint32_t nr_txndata_prepend_metric(nr_flatbuffer_t* fb,
                                          const nrmtable_t* table,
                                          const nrmetric_t* metric,
                                          int scoped) {
  uint32_t name;
  uint32_t data;

  name = nr_flatbuffers_prepend_string(fb, nrm_get_name(table, metric));

  nr_flatbuffers_object_begin(fb, METRIC_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, METRIC_FIELD_NAME, name, 0);
  data = nr_txndata_prepend_metric_data(fb, metric, scoped);
  nr_flatbuffers_object_prepend_struct(fb, METRIC_FIELD_DATA, data, 0);
  return nr_flatbuffers_object_end(fb);
}

static uint32_t nr_txndata_prepend_metrics(nr_flatbuffer_t* fb,
                                           const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t metrics;
  int num_scoped;
  int num_unscoped;
  int num_metrics;
  int i;

  num_scoped = nrm_table_size(txn->scoped_metrics);
  num_unscoped = nrm_table_size(txn->unscoped_metrics);
  num_metrics = num_scoped + num_unscoped;

  if (0 == num_metrics) {
    return 0;
  }

  offsets = (uint32_t*)nr_calloc(num_metrics, sizeof(uint32_t));
  offset = &offsets[0];

  for (i = 0; i < num_unscoped; i++, offset++) {
    const nrmetric_t* metric;

    metric = nrm_get_metric(txn->unscoped_metrics, i);
    *offset = nr_txndata_prepend_metric(fb, txn->unscoped_metrics, metric, 0);
  }

  for (i = 0; i < num_scoped; i++, offset++) {
    const nrmetric_t* metric;

    metric = nrm_get_metric(txn->scoped_metrics, i);
    *offset = nr_txndata_prepend_metric(fb, txn->scoped_metrics, metric, 1);
  }

  nr_flatbuffers_vector_begin(fb, sizeof(uint32_t), num_metrics,
                              sizeof(uint32_t));
  for (i = 0; i < num_metrics; i++) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  metrics = nr_flatbuffers_vector_end(fb, num_metrics);

  nr_free(offsets);
  return metrics;
}

static uint32_t nr_txndata_prepend_slowsqls(nr_flatbuffer_t* fb,
                                            const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t slowsqls;
  int i;
  int slowsql_count;

  const size_t slowsql_size = sizeof(uint32_t);
  const size_t slowsql_align = sizeof(uint32_t);

  slowsql_count = nr_slowsqls_saved(txn->slowsqls);
  if (0 == slowsql_count) {
    return 0;
  }

  offsets = (uint32_t*)nr_calloc(slowsql_count, sizeof(uint32_t));
  offset = &offsets[0];

  for (i = slowsql_count - 1; i >= 0; i--, offset++) {
    const nr_slowsql_t* slow;
    uint32_t params;
    uint32_t query;
    uint32_t metric;

    slow = nr_slowsqls_at(txn->slowsqls, i);
    params = nr_flatbuffers_prepend_string(fb, nr_slowsql_params(slow));
    query = nr_flatbuffers_prepend_string(fb, nr_slowsql_query(slow));
    metric = nr_flatbuffers_prepend_string(fb, nr_slowsql_metric(slow));

    nr_flatbuffers_object_begin(fb, SLOWSQL_NUM_FIELDS);
    nr_flatbuffers_object_prepend_uoffset(fb, SLOWSQL_FIELD_PARAMS, params, 0);
    nr_flatbuffers_object_prepend_uoffset(fb, SLOWSQL_FIELD_QUERY, query, 0);
    nr_flatbuffers_object_prepend_uoffset(fb, SLOWSQL_FIELD_METRIC, metric, 0);
    nr_flatbuffers_object_prepend_u64(fb, SLOWSQL_FIELD_MAX_MICROS,
                                      nr_slowsql_max(slow) / NR_TIME_DIVISOR_US,
                                      0);
    nr_flatbuffers_object_prepend_u64(fb, SLOWSQL_FIELD_MIN_MICROS,
                                      nr_slowsql_min(slow) / NR_TIME_DIVISOR_US,
                                      0);
    nr_flatbuffers_object_prepend_u64(
        fb, SLOWSQL_FIELD_TOTAL_MICROS,
        nr_slowsql_total(slow) / NR_TIME_DIVISOR_US, 0);
    nr_flatbuffers_object_prepend_i32(fb, SLOWSQL_FIELD_COUNT,
                                      nr_slowsql_count(slow), 0);
    nr_flatbuffers_object_prepend_u32(fb, SLOWSQL_FIELD_ID, nr_slowsql_id(slow),
                                      0);
    *offset = nr_flatbuffers_object_end(fb);
  }

  nr_flatbuffers_vector_begin(fb, slowsql_size, slowsql_count, slowsql_align);
  for (i = slowsql_count - 1; i >= 0; i--) {
    nr_flatbuffers_prepend_uoffset(fb, offsets[i]);
  }
  slowsqls = nr_flatbuffers_vector_end(fb, slowsql_count);

  nr_free(offsets);
  return slowsqls;
}

static uint32_t nr_txndata_prepend_error_events(nr_flatbuffer_t* fb,
                                                const nrtxn_t* txn) {
  uint32_t* offsets;
  uint32_t* offset;
  uint32_t events;
  int event_count;
  nr_analytics_event_t* event;
  const char* json;
  uint32_t data;

  const size_t event_size = sizeof(uint32_t);
  const size_t event_align = sizeof(uint32_t);

  event = nr_error_to_event(txn);
  if (NULL == event) {
    return 0;
  }

  /*
   * Currently there is only one error captured per transaction, but we write
   * it as a vector in preparation for a future where multiple errors are kept.
   */
  event_count = 1;
  offsets = (uint32_t*)nr_calloc(event_count, sizeof(uint32_t));
  offset = &offsets[0];

  json = nr_analytics_event_json(event);
  data = nr_flatbuffers_prepend_string(fb, json);
  nr_analytics_event_destroy(&event);

  nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
  *offset = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_vector_begin(fb, event_size, event_count, event_align);
  nr_flatbuffers_prepend_uoffset(fb, offsets[0]);
  events = nr_flatbuffers_vector_end(fb, event_count);

  nr_free(offsets);
  return events;
}

static uint32_t nr_txndata_prepend_trace_to_flatbuffer(nr_flatbuffer_t* fb,
                                                       const nrtxn_t* txn) {
  double duration_ms;
  double timestamp_ms;
  uint32_t data;
  uint32_t guid;
  int force_persist;

  if (NULL == txn->final_data.trace_json) {
    return 0;
  }

  data = nr_flatbuffers_prepend_string(fb, txn->final_data.trace_json);
  guid = nr_flatbuffers_prepend_string(fb, nr_txn_get_guid(txn));

  timestamp_ms = nr_txn_start_time(txn) / NR_TIME_DIVISOR_MS_D;
  duration_ms = nr_txn_duration(txn) / NR_TIME_DIVISOR_MS_D;
  force_persist = nr_txn_should_force_persist(txn);

  nr_flatbuffers_object_begin(fb, TRACE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, TRACE_FIELD_DATA, data, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRACE_FIELD_GUID, guid, 0);
  nr_flatbuffers_object_prepend_bool(fb, TRACE_FIELD_FORCE_PERSIST,
                                     force_persist, 0);
  nr_flatbuffers_object_prepend_f64(fb, TRACE_FIELD_DURATION, duration_ms, 0);
  nr_flatbuffers_object_prepend_f64(fb, TRACE_FIELD_TIMESTAMP, timestamp_ms, 0);
  return nr_flatbuffers_object_end(fb);
}

static uint32_t nr_txndata_prepend_txn_event(nr_flatbuffer_t* fb,
                                             const nrtxn_t* txn) {
  nr_analytics_event_t* event;
  const char* json;
  uint32_t data;

  event = nr_txn_to_event(txn);
  if (NULL == event) {
    return 0;
  }

  json = nr_analytics_event_json(event);
  data = nr_flatbuffers_prepend_string(fb, json);
  nr_analytics_event_destroy(&event);

  nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
  return nr_flatbuffers_object_end(fb);
}

static uint32_t nr_txndata_prepend_synthetics_resource_id(nr_flatbuffer_t* fb,
                                                          const nrtxn_t* txn) {
  const char* synthetics_resource_id;

  synthetics_resource_id = nr_synthetics_resource_id(txn->synthetics);
  if (synthetics_resource_id) {
    return nr_flatbuffers_prepend_string(fb, synthetics_resource_id);
  }
  return 0;
}

static uint32_t nr_txndata_prepend_request_uri(nr_flatbuffer_t* fb,
                                               const nrtxn_t* txn) {
  if (txn->request_uri) {
    return nr_flatbuffers_prepend_string(fb, txn->request_uri);
  }
  return nr_flatbuffers_prepend_string(fb, "<unknown>");
}

static uint32_t nr_txndata_prepend_php_packages(nr_flatbuffer_t* fb,
                                                const nrtxn_t* txn) {
  uint32_t data;
  char* json;
  int packages_count;

  packages_count = nr_php_packages_count(txn->php_packages);
  if (0 == packages_count) {
    return 0;
  }

  json = nr_php_packages_to_json(txn->php_packages);
  if (NULL == json) {
    return 0;
  }

  nrl_verbosedebug(NRL_DEBUG, "php packages json = |%s|", json);

  data = nr_flatbuffers_prepend_string(fb, json);
  nr_free(json);

  nr_flatbuffers_object_begin(fb, EVENT_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, EVENT_FIELD_DATA, data, 0);
  return nr_flatbuffers_object_end(fb);
}

static uint32_t nr_txndata_prepend_transaction(nr_flatbuffer_t* fb,
                                               const nrtxn_t* txn,
                                               int32_t pid) {
  uint32_t custom_events;
  uint32_t error_events;
  uint32_t errors;
  uint32_t metrics;
  uint32_t name;
  uint32_t request_uri;
  uint32_t resource_id;
  uint32_t slowsqls;
  uint32_t txn_event;
  uint32_t txn_trace;
  uint32_t span_events;
  uint32_t log_events;
  uint32_t php_packages;

  txn_trace = nr_txndata_prepend_trace_to_flatbuffer(fb, txn);
  span_events = nr_txndata_prepend_span_events(fb, txn->final_data.span_events,
                                               txn->app_limits.span_events);
  log_events
      = nr_txndata_prepend_log_events(fb, txn, txn->app_limits.log_events);
  error_events = nr_txndata_prepend_error_events(fb, txn);
  custom_events = nr_txndata_prepend_custom_events(fb, txn);
  slowsqls = nr_txndata_prepend_slowsqls(fb, txn);
  errors = nr_txndata_prepend_errors(fb, txn);
  metrics = nr_txndata_prepend_metrics(fb, txn);
  php_packages = nr_txndata_prepend_php_packages(fb, txn);
  txn_event = nr_txndata_prepend_txn_event(fb, txn);
  resource_id = nr_txndata_prepend_synthetics_resource_id(fb, txn);
  request_uri = nr_txndata_prepend_request_uri(fb, txn);
  name = nr_flatbuffers_prepend_string(fb, txn->name);

  nr_flatbuffers_object_begin(fb, TRANSACTION_NUM_FIELDS);
  nr_flatbuffers_object_prepend_f64(
      fb, TRANSACTION_FIELD_SAMPLING_PRIORITY,
      (double)nr_distributed_trace_get_priority(txn->distributed_trace), 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_ERROR_EVENTS,
                                        error_events, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_TRACE, txn_trace,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_CUSTOM_EVENTS,
                                        custom_events, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_SLOW_SQLS,
                                        slowsqls, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_ERRORS, errors,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_METRICS, metrics,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_TXN_EVENT,
                                        txn_event, 0);
  nr_flatbuffers_object_prepend_i32(fb, TRANSACTION_FIELD_PID, pid, 0);
  nr_flatbuffers_object_prepend_uoffset(
      fb, TRANSACTION_FIELD_SYNTHETICS_RESOURCE_ID, resource_id, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_URI, request_uri,
                                        0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_NAME, name, 0);

  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_SPAN_EVENTS,
                                        span_events, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_LOG_EVENTS,
                                        log_events, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, TRANSACTION_FIELD_PHP_PACKAGES,
                                        php_packages, 0);
  return nr_flatbuffers_object_end(fb);
}

nr_flatbuffer_t* nr_txndata_encode(const nrtxn_t* txn) {
  nr_flatbuffer_t* fb;
  uint32_t message;
  uint32_t agent_run_id;
  uint32_t transaction;

  fb = nr_flatbuffers_create(0);
  transaction = nr_txndata_prepend_transaction(fb, txn, (int32_t)nr_getpid());
  agent_run_id = nr_flatbuffers_prepend_string(fb, txn->agent_run_id);

  nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_DATA, transaction, 0);
  nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                   MESSAGE_BODY_TXN, 0);
  nr_flatbuffers_object_prepend_uoffset(fb, MESSAGE_FIELD_AGENT_RUN_ID,
                                        agent_run_id, 0);
  message = nr_flatbuffers_object_end(fb);

  nr_flatbuffers_finish(fb, message);

  return fb;
}

/* Hook for stubbing TXNDATA messages during testing. */
nr_status_t (*nr_cmd_txndata_hook)(int daemon_fd, const nrtxn_t* txn) = NULL;

/*
 * This timeout will delay the process, but the request has finished,
 * so this will not impact response time.  Therefore this is not as important as
 * the timeout in appinfo.  However, it will prevent this process from handling
 * a new request, so it will have some impact.
 */
#define NR_TXNDATA_SEND_TIMEOUT_MSEC 500

nr_status_t nr_cmd_txndata_tx(int daemon_fd, const nrtxn_t* txn) {
  nr_flatbuffer_t* msg;
  size_t msglen;
  nr_status_t st;

  if (nr_cmd_txndata_hook) {
    return nr_cmd_txndata_hook(daemon_fd, txn);
  }

  if ((NULL == txn) || (daemon_fd < 0)) {
    return NR_FAILURE;
  }

  nrl_verbosedebug(
      NRL_TXN,
      "sending txnname='%.64s'"
      " agent_run_id=" NR_AGENT_RUN_ID_FMT
      " segment_count=%zu"
      " duration=" NR_TIME_FMT " threshold=" NR_TIME_FMT " priority=%f",
      txn->name ? txn->name : "unknown", txn->agent_run_id, txn->segment_count,
      nr_txn_duration(txn), txn->options.tt_threshold,
      (double)nr_distributed_trace_get_priority(txn->distributed_trace));

  msg = nr_txndata_encode(txn);
  msglen = nr_flatbuffers_len(msg);

  nrl_verbosedebug(NRL_DAEMON, "sending transaction message, len=%zu", msglen);

  if (nr_command_is_flatbuffer_invalid(msg, msglen)) {
    nr_flatbuffers_destroy(&msg);
    return NR_FAILURE;
  }

  nr_agent_lock_daemon_mutex();
  {
    nrtime_t deadline;

    deadline
        = nr_get_time() + (NR_TXNDATA_SEND_TIMEOUT_MSEC * NR_TIME_DIVISOR_MS);
    st = nr_write_message(daemon_fd, nr_flatbuffers_data(msg), msglen,
                          deadline);
  }
  nr_agent_unlock_daemon_mutex();
  nr_flatbuffers_destroy(&msg);

  if (NR_SUCCESS != st) {
    nrl_error(NRL_DAEMON, "TXNDATA failure: len=%zu errno=%s", msglen,
              nr_errno(errno));
    nr_agent_close_daemon_connection();
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}
