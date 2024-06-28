/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "nr_distributed_trace.h"
#include "nr_distributed_trace_private.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_regex.h"
#include "util_time.h"
#include "util_strings.h"
#include "util_logging.h"

/*
 * Purpose : Helper function to assign a string value to a field.  This function
 *           is handy because nr_strdup() will allocate memory to have a copy
 *           of the value passed in.  That memory needs to be nr_free()'d before
 *           the field can be changed. Secondly nr_strdup() returns an empty
 *           string "" when NULL is passed in as a parameter.  The desired
 *           outcome is for a NULL value to be passed along to the field as-is.
 *
 * Params :  1. The string field to override
 *           2. The string value to copy into field
 *
 * Returns : None
 */
static inline void set_dt_field(char** field, const char* value) {
  nr_free(*field);

  if (!nr_strempty(value)) {
    *field = nr_strdup(value);
  }
}

/*
 * Purpose      Format trace's priority for tracestate in W3C header.
 *
 * @param       value   double
 * @return      char*   priority string buffer
 */
static char* nr_priority_double_to_str(nr_sampling_priority_t value) {
  char* buf = NULL;

  buf = nr_formatf("%.6f", value);

  for (int i = 0; i < nr_strlen(buf); i++) {
    if (',' == buf[i]) {
      buf[i] = '.';
      break;
    }
  }

  return buf;
}

nr_distributed_trace_t* nr_distributed_trace_create(void) {
  nr_distributed_trace_t* dt;
  dt = (nr_distributed_trace_t*)nr_zalloc(sizeof(nr_distributed_trace_t));

  // Set any non-zero default values here.

  return dt;
}

bool nr_distributed_trace_accept_inbound_payload(nr_distributed_trace_t* dt,
                                                 const nrobj_t* obj_payload,
                                                 const char* transport_type,
                                                 const char** error) {
  const nrobj_t* obj_payload_data;
  nr_status_t errp = NR_FAILURE;
  nr_sampling_priority_t priority;
  bool sampled;

  if (NULL != *error) {
    return false;
  }

  if (NULL == dt) {
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_EXCEPTION;
    return false;
  }

  if (NULL == obj_payload) {
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION;
    return false;
  }

  obj_payload_data = nro_get_hash_hash(obj_payload, "d", NULL);

  set_dt_field(&dt->inbound.type,
               nro_get_hash_string(obj_payload_data, "ty", NULL));
  set_dt_field(&dt->inbound.account_id,
               nro_get_hash_string(obj_payload_data, "ac", NULL));
  set_dt_field(&dt->inbound.app_id,
               nro_get_hash_string(obj_payload_data, "ap", NULL));
  set_dt_field(&dt->inbound.guid,
               nro_get_hash_string(obj_payload_data, "id", NULL));
  set_dt_field(&dt->inbound.txn_id,
               nro_get_hash_string(obj_payload_data, "tx", NULL));
  set_dt_field(&dt->trace_id,
               nro_get_hash_string(obj_payload_data, "tr", NULL));

  /*
   * Keep the current priority if the priority in the inbound payload is
   * missing or invalid.
   */
  priority = (nr_sampling_priority_t)nro_get_hash_double(obj_payload_data, "pr",
                                                         &errp);
  if (NR_SUCCESS == errp) {
    dt->priority = priority;
  }

  /*
   * Keep the current sampled flag if the sampled flag in the inbound payload is
   * missing or invalid.
   */
  sampled = nro_get_hash_boolean(obj_payload_data, "sa", &errp);

  if (NR_SUCCESS == errp) {
    dt->sampled = sampled;
  }

  // Convert payload timestamp from MS to US.
  dt->inbound.timestamp
      = ((nrtime_t)nro_get_hash_long(obj_payload_data, "ti", NULL))
        * NR_TIME_DIVISOR_MS;

  nr_distributed_trace_inbound_set_transport_type(dt, transport_type);
  dt->inbound.set = true;

  return true;
}

nrobj_t* nr_distributed_trace_convert_payload_to_object(const char* payload,
                                                        const char** error) {
  nrobj_t* obj_payload;
  const nrobj_t* obj_payload_version;
  const nrobj_t* obj_payload_data;
  const char* required_data_fields[] = {"ty", "ac", "ap", "tr", "ti"};
  nr_status_t err;
  nr_status_t tx_err;
  size_t i = 0;

  if (NULL != *error) {
    return NULL;
  }

  if (nr_strempty(payload)) {
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_NULL;
    return NULL;
  }

  obj_payload = nro_create_from_json(payload);

  if (NULL == obj_payload) {
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION;
    return NULL;
  }

  obj_payload_version = nro_get_hash_array(obj_payload, "v", NULL);

  // Version missing
  if (NULL == obj_payload_version) {
    nrl_debug(NRL_CAT,
              "Inbound distributed tracing payload invalid. Missing version.");
    nro_delete(obj_payload);
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION;
    return NULL;
  }

  // Compare version major
  if (nro_get_array_int(obj_payload_version, 1, NULL)
      > NR_DISTRIBUTED_TRACE_VERSION_MAJOR) {
    nrl_debug(
        NRL_CAT,
        "Inbound distributed tracing payload invalid. Unexpected version: the "
        "maximum version supported is %d, but the payload has version %d.",
        NR_DISTRIBUTED_TRACE_VERSION_MAJOR,
        nro_get_array_int(obj_payload_version, 1, NULL));
    nro_delete(obj_payload);
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_MAJOR_VERSION;
    return NULL;
  }

  obj_payload_data = nro_get_hash_hash(obj_payload, "d", NULL);

  // Check that at least one of guid or transactionId are present
  nro_get_hash_string(obj_payload_data, "tx", &tx_err);
  nro_get_hash_string(obj_payload_data, "id", &err);
  if (NR_FAILURE == err && NR_FAILURE == tx_err) {
    nrl_debug(
        NRL_CAT,
        "Inbound distributed tracing payload format invalid. Missing both "
        "guid (d.id) and transactionId (d.tx).");
    *error = NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION;
    nro_delete(obj_payload);
    return NULL;
  }

  // Check required fields for their key presence
  for (i = 0;
       i < sizeof(required_data_fields) / sizeof(required_data_fields[0]);
       i++) {
    nro_get_hash_string(obj_payload_data, required_data_fields[i], &err);
    if (NR_FAILURE == err) {
      nro_get_hash_long(obj_payload_data, required_data_fields[i], &err);
      if (NR_FAILURE == err) {
        nrl_debug(NRL_CAT,
                  "Inbound distributed tracing payload format invalid. "
                  "Missing field '%s'",
                  required_data_fields[i]);
        *error = NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION;
        nro_delete(obj_payload);
        return NULL;
      }
    }
  }

  return obj_payload;
}

void nr_distributed_trace_destroy(nr_distributed_trace_t** ptr) {
  nr_distributed_trace_t* trace = NULL;

  if ((NULL == ptr) || (NULL == *ptr)) {
    return;
  }

  trace = *ptr;
  nr_free(trace->account_id);
  nr_free(trace->app_id);
  nr_free(trace->txn_id);
  nr_free(trace->trace_id);
  nr_free(trace->trusted_key);

  nr_free(trace->inbound.type);
  nr_free(trace->inbound.app_id);
  nr_free(trace->inbound.account_id);
  nr_free(trace->inbound.transport_type);
  nr_free(trace->inbound.guid);
  nr_free(trace->inbound.txn_id);
  nr_free(trace->inbound.tracing_vendors);
  nr_free(trace->inbound.raw_tracing_vendors);
  nr_free(trace->inbound.trusted_parent_id);

  nr_realfree((void**)ptr);
}

const char* nr_distributed_trace_get_account_id(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->account_id;
}

const char* nr_distributed_trace_get_trusted_key(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->trusted_key;
}

const char* nr_distributed_trace_get_app_id(const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->app_id;
}

const char* nr_distributed_trace_get_txn_id(const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->txn_id;
}
nr_sampling_priority_t nr_distributed_trace_get_priority(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NR_PRIORITY_ERROR;
  }

  return dt->priority;
}

const char* nr_distributed_trace_get_trace_id(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->trace_id;
}

const char* nr_distributed_trace_inbound_get_tracing_vendors(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.tracing_vendors;
}

const char* nr_distributed_trace_inbound_get_raw_tracing_vendors(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.raw_tracing_vendors;
}

const char* nr_distributed_trace_inbound_get_trusted_parent_id(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.trusted_parent_id;
}

bool nr_distributed_trace_is_sampled(const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return false;
  }
  return dt->sampled;
}

bool nr_distributed_trace_inbound_is_set(const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return false;
  }

  return dt->inbound.set;
}

const char* nr_distributed_trace_inbound_get_account_id(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.account_id;
}

const char* nr_distributed_trace_inbound_get_app_id(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.app_id;
}

const char* nr_distributed_trace_inbound_get_guid(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.guid;
}

const char* nr_distributed_trace_inbound_get_txn_id(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.txn_id;
}

const char* nr_distributed_trace_inbound_get_type(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.type;
}

nrtime_t nr_distributed_trace_inbound_get_timestamp_delta(
    const nr_distributed_trace_t* dt,
    nrtime_t txn_start) {
  if (NULL == dt) {
    return 0;
  }

  return nr_time_duration(dt->inbound.timestamp, txn_start);
}

extern bool nr_distributed_trace_inbound_has_timestamp(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return 0;
  }

  return dt->inbound.timestamp != 0;
}

const char* nr_distributed_trace_inbound_get_transport_type(
    const nr_distributed_trace_t* dt) {
  if (NULL == dt) {
    return NULL;
  }

  return dt->inbound.transport_type;
}

const char* nr_distributed_trace_object_get_account_id(const nrobj_t* object) {
  const nrobj_t* obj_payload_data;
  obj_payload_data = nro_get_hash_hash(object, "d", NULL);
  return nro_get_hash_string(obj_payload_data, "ac", NULL);
}

const char* nr_distributed_trace_object_get_trusted_key(const nrobj_t* object) {
  const nrobj_t* obj_payload_data;
  obj_payload_data = nro_get_hash_hash(object, "d", NULL);
  return nro_get_hash_string(obj_payload_data, "tk", NULL);
}

void nr_distributed_trace_set_txn_id(nr_distributed_trace_t* dt,
                                     const char* txn_id) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->txn_id);
  if (txn_id) {
    dt->txn_id = nr_strdup(txn_id);
  }
}

void nr_distributed_trace_set_trusted_key(nr_distributed_trace_t* dt,
                                          const char* trusted_key) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->trusted_key);
  if (trusted_key) {
    dt->trusted_key = nr_strdup(trusted_key);
  }
}

void nr_distributed_trace_set_account_id(nr_distributed_trace_t* dt,
                                         const char* account_id) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->account_id);
  if (account_id) {
    dt->account_id = nr_strdup(account_id);
  }
}

void nr_distributed_trace_set_app_id(nr_distributed_trace_t* dt,
                                     const char* app_id) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->app_id);
  if (app_id) {
    dt->app_id = nr_strdup(app_id);
  }
}

void nr_distributed_trace_set_trace_id(nr_distributed_trace_t* dt,
                                       const char* trace_id) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->trace_id);
  if (trace_id) {
    int len = nr_strlen(trace_id);
    if (len < NR_TRACE_ID_SIZE) {
      int padding = NR_TRACE_ID_SIZE - len;
      char* dest = (char*)(malloc)(NR_TRACE_ID_SIZE + 1);
      snprintf(dest, NR_TRACE_ID_SIZE+1, "%0*d%s", padding, 0, trace_id);
      dt->trace_id = dest;
    } else {
      dt->trace_id = nr_strdup(trace_id);
    }
  }
}

void nr_distributed_trace_set_priority(nr_distributed_trace_t* dt,
                                       nr_sampling_priority_t priority) {
  if (NULL == dt) {
    return;
  }

  dt->priority = priority;
}

void nr_distributed_trace_inbound_set_tracing_vendors(
    nr_distributed_trace_t* dt,
    const char* other_vendors) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->inbound.tracing_vendors);
  if (other_vendors) {
    dt->inbound.tracing_vendors = nr_strdup(other_vendors);
  }
}

void nr_distributed_trace_inbound_set_trusted_parent_id(
    nr_distributed_trace_t* dt,
    const char* trusted_parent_id) {
  if (NULL == dt) {
    return;
  }

  nr_free(dt->inbound.trusted_parent_id);
  if (trusted_parent_id) {
    dt->inbound.trusted_parent_id = nr_strdup(trusted_parent_id);
  }
}

void nr_distributed_trace_set_sampled(nr_distributed_trace_t* dt, bool value) {
  if (NULL == dt) {
    return;
  }
  dt->sampled = value;
}

void nr_distributed_trace_inbound_set_transport_type(nr_distributed_trace_t* dt,
                                                     const char* value) {
  static const char* supported_types[]
      = {"Unknown", "HTTP", "HTTPS", "Kafka", "JMS",
         "IronMQ",  "AMQP", "Queue", "Other"};

  if (NULL == dt) {
    return;
  }

  for (unsigned i = 0; i < sizeof(supported_types) / sizeof(supported_types[0]);
       i++) {
    if (0 == nr_strcmp(value, supported_types[i])) {
      set_dt_field(&dt->inbound.transport_type, value);
      return;
    }
  }

  nrl_verbosedebug(NRL_CAT, "Unknown transport type in %s: %s", __func__,
                   value ? value : "(null)");

  set_dt_field(&dt->inbound.transport_type, "Unknown");
}

nr_distributed_trace_payload_t* nr_distributed_trace_payload_create(
    const nr_distributed_trace_t* metadata,
    const char* parent_id) {
  nr_distributed_trace_payload_t* p;
  p = (nr_distributed_trace_payload_t*)nr_zalloc(
      sizeof(nr_distributed_trace_payload_t));

  p->metadata = metadata;
  p->timestamp = nr_get_time();

  if (parent_id) {
    p->parent_id = nr_strdup(parent_id);
  }

  return p;
}

void nr_distributed_trace_payload_destroy(
    nr_distributed_trace_payload_t** ptr) {
  nr_distributed_trace_payload_t* payload;
  if (NULL == ptr || NULL == *ptr) {
    return;
  }

  payload = *ptr;

  nr_free(payload->parent_id);

  nr_realfree((void**)ptr);
}

const char* nr_distributed_trace_payload_get_parent_id(
    const nr_distributed_trace_payload_t* payload) {
  if (NULL == payload) {
    return NULL;
  }

  return payload->parent_id;
}

nrtime_t nr_distributed_trace_payload_get_timestamp(
    const nr_distributed_trace_payload_t* payload) {
  if (NULL == payload) {
    return 0;
  }

  return payload->timestamp;
}

const nr_distributed_trace_t* nr_distributed_trace_payload_get_metadata(
    const nr_distributed_trace_payload_t* payload) {
  if (NULL == payload) {
    return NULL;
  }

  return payload->metadata;
}

static inline void add_field_if_set(nrobj_t* obj,
                                    const char* key,
                                    const char* value) {
  if (value) {
    nro_set_hash_string(obj, key, value);
  }
}

char* nr_distributed_trace_payload_as_text(
    const nr_distributed_trace_payload_t* payload) {
  nrobj_t* data;
  nrobj_t* obj;
  char* text;
  nrobj_t* version;

  if ((NULL == payload) || (NULL == payload->metadata)) {
    return NULL;
  }

  if (NULL == payload->parent_id && NULL == payload->metadata->txn_id) {
    return NULL;
  }

  obj = nro_new_hash();

  version = nro_new_array();
  nro_set_array_int(version, 0, NR_DISTRIBUTED_TRACE_VERSION_MAJOR);
  nro_set_array_int(version, 0, NR_DISTRIBUTED_TRACE_VERSION_MINOR);
  nro_set_hash(obj, "v", version);
  nro_delete(version);

  data = nro_new_hash();
  nro_set_hash_string(data, "ty", "App");
  add_field_if_set(data, "ac", payload->metadata->account_id);
  add_field_if_set(data, "ap", payload->metadata->app_id);

  add_field_if_set(data, "id", payload->parent_id);
  add_field_if_set(data, "tr", payload->metadata->trace_id);
  add_field_if_set(data, "tx", payload->metadata->txn_id);
  nro_set_hash_double(data, "pr", payload->metadata->priority);
  nro_set_hash_boolean(data, "sa", payload->metadata->sampled);
  nro_set_hash_long(data, "ti",
                    (long)(payload->timestamp / NR_TIME_DIVISOR_MS));

  /*
   * According to the spec the trusted key is relevant only when it differs
   * from the account id.
   */
  if (0
      != nr_strcmp(payload->metadata->trusted_key,
                   payload->metadata->account_id)) {
    add_field_if_set(data, "tk", payload->metadata->trusted_key);
  }
  nro_set_hash(obj, "d", data);
  nro_delete(data);

  text = nro_to_json(obj);
  nro_delete(obj);

  return text;
}

static inline void nr_distributed_trace_set_parent_type(
    nr_distributed_trace_t* dt,
    int w3c_type) {
  switch (w3c_type) {
    case 0:
      set_dt_field(&dt->inbound.type, "App");
      return;
    case 1:
      set_dt_field(&dt->inbound.type, "Browser");
      return;
    case 2:
      set_dt_field(&dt->inbound.type, "Mobile");
      return;
    default:
      set_dt_field(&dt->inbound.type, "App");
  }
}

static inline void nr_distributed_trace_accept_tracestate(
    nr_distributed_trace_t* dt,
    const nrobj_t* tracestate) {
  int ts_parent_type;
  const char* ts_account_id = NULL;
  const char* ts_app_id = NULL;
  const char* ts_txn_id = NULL;
  bool ts_sampled;
  double ts_priority = 0;
  nr_status_t* parse_err = 0;
  const char* ts_span_id
      = nro_get_hash_string(tracestate, "span_id", parse_err);

  if (NULL != ts_span_id) {
    nr_distributed_trace_inbound_set_trusted_parent_id(dt, ts_span_id);
  }

  // Account ID is required, it's not likely to be NULL.
  ts_account_id
      = nro_get_hash_string(tracestate, "parent_account_id", parse_err);
  if (nrunlikely(NULL != ts_account_id && NR_SUCCESS == parse_err)) {
    set_dt_field(&dt->inbound.account_id, ts_account_id);
  }

  ts_app_id
      = nro_get_hash_string(tracestate, "parent_application_id", parse_err);
  if (nrunlikely(NULL != ts_app_id && NR_SUCCESS == parse_err)) {
    set_dt_field(&dt->inbound.app_id, ts_app_id);
  }

  ts_txn_id = nro_get_hash_string(tracestate, "transaction_id", parse_err);
  if (NULL != ts_txn_id && NR_SUCCESS == parse_err) {
    set_dt_field(&dt->inbound.txn_id, ts_txn_id);
  }

  ts_sampled = nro_get_hash_int(tracestate, "sampled", parse_err);
  if (NR_SUCCESS == parse_err) {
    dt->sampled = ts_sampled;
  }

  ts_priority = nro_get_hash_double(tracestate, "priority", parse_err);
  if (NR_SUCCESS == parse_err && 0 < ts_priority) {
    dt->priority = ts_priority;
  }

  dt->inbound.timestamp
      = (nrtime_t)nro_get_hash_long(tracestate, "timestamp", parse_err)
        * NR_TIME_DIVISOR_MS;

  ts_parent_type = nro_get_hash_int(tracestate, "parent_type", parse_err);
  if (NR_SUCCESS == parse_err) {
    nr_distributed_trace_set_parent_type(dt, ts_parent_type);
  }
}

bool nr_distributed_trace_accept_inbound_w3c_payload(
    nr_distributed_trace_t* dt,
    const nrobj_t* trace_headers,
    const char* transport_type,
    const char** error) {
  const nrobj_t* tracestate = NULL;
  const nrobj_t* traceparent = NULL;
  const char* tracingVendors = NULL;
  const char* rawTracingVendors = NULL;
  nr_status_t* parse_err = 0;

  const char* tp_trace_id = NULL;
  const char* tp_span_id = NULL;

  if (nrunlikely(NULL == error || NULL != *error)) {
    return false;
  }

  if (NULL == dt) {
    *error = NR_DISTRIBUTED_TRACE_W3C_TRACECONTEXT_ACCEPT_EXCEPTION;
    return false;
  }

  if (NULL == trace_headers) {
    *error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    return false;
  }

  traceparent = nro_get_hash_value(trace_headers, "traceparent", parse_err);

  if (nrunlikely(NULL == traceparent || NR_SUCCESS != parse_err)) {
    *error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    return false;
  }

  tp_span_id = nro_get_hash_string(traceparent, "parent_id", parse_err);

  // The trace parent span ID is required.
  if (nrunlikely(NR_SUCCESS != parse_err || NULL == tp_span_id)) {
    *error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    return false;
  }

  tp_trace_id = nro_get_hash_string(traceparent, "trace_id", parse_err);
  if (nrunlikely(NR_SUCCESS != parse_err || NULL == tp_trace_id)) {
    *error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    return false;
  }

  tracestate = nro_get_hash_value(trace_headers, "tracestate", parse_err);
  // When a trace starts with another vendor we won't have a valid tracestate.
  // This is still a valid trace.
  if (tracestate && NR_SUCCESS == parse_err) {
    nr_distributed_trace_accept_tracestate(dt, tracestate);
  }

  tracingVendors
      = nro_get_hash_string(trace_headers, "tracingVendors", parse_err);
  if (NULL != tracingVendors) {
    set_dt_field(&dt->inbound.tracing_vendors, tracingVendors);
  }

  rawTracingVendors
      = nro_get_hash_string(trace_headers, "rawTracingVendors", parse_err);
  if (NULL != rawTracingVendors) {
    set_dt_field(&dt->inbound.raw_tracing_vendors, rawTracingVendors);
  }

  nr_distributed_trace_inbound_set_transport_type(dt, transport_type);
  set_dt_field(&dt->inbound.guid, tp_span_id);

  set_dt_field(&dt->trace_id, tp_trace_id);

  dt->inbound.set = true;
  return true;
}

/*
 * Purpose : Parse a W3C trace parent header.
 *
 *           Refer to this specification for further details:
 *           https://w3c.github.io/trace-context/#traceparent-header
 */
static const char* nr_distributed_trace_convert_w3c_headers_traceparent(
    nrobj_t* obj,
    const char* traceparent) {
  nrobj_t* traceparent_obj = NULL;
  char* str = NULL;
  char* additional = NULL;
  const char* error = NULL;
  nr_regex_t* regex = NULL;
  nr_regex_substrings_t* ss = NULL;

  if (NULL == traceparent || NULL == obj) {
    nrl_debug(NRL_CAT, "Inbound W3C trace parent: NULL given");
    error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    goto end;
  }

  /* Note: the W3C Trace Context spec indicates lowercase alpha characters
   * in all hex values */
  regex = nr_regex_create(
      "^"
      "(?P<version>[0-9a-f]{2})-"
      "(?P<trace_id>[0-9a-f]{32})-"
      "(?P<parent_id>[0-9a-f]{16})-"
      "(?P<trace_flags>[0-9a-f]{2})"
      "(?P<additional>-.*)?$",
      0, 0);

  ss = nr_regex_match_capture(regex, traceparent, strlen(traceparent));

  if (NULL == ss) {
    nrl_warning(NRL_CAT, "Inbound W3C trace parent invalid: cannot parse '%s'",
                traceparent);
    error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    goto end;
  }

  traceparent_obj = nro_new_hash();

  str = nr_regex_substrings_get_named(ss, "version");
  if (0 == nr_strcmp(str, "ff")) {
    nrl_warning(NRL_CAT,
                "Inbound W3C trace parent invalid: version 0xff is forbidden");
    error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    nr_free(str);
    goto end;
  }
  additional = nr_regex_substrings_get_named(ss, "additional");
  if (0 == nr_strcmp(str, "00") && NULL != additional) {
    nrl_warning(NRL_CAT,
                "Inbound W3C trace parent invalid: received additional fields "
                "that are not valid for trace parent version 00");
    error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    nr_free(str);
    nr_free(additional);
    goto end;
  }
  nro_set_hash_string(traceparent_obj, "version", str);
  nr_free(str);
  nr_free(additional);

  str = nr_regex_substrings_get_named(ss, "trace_id");
  if (0 == nr_strcmp(str, "00000000000000000000000000000000")) {
    nrl_warning(NRL_CAT, "Inbound W3C trace parent invalid: trace id '%s'",
                str);
    nr_free(str);
    error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    goto end;
  }
  nro_set_hash_string(traceparent_obj, "trace_id", str);
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "parent_id");
  if (0 == nr_strcmp(str, "0000000000000000")) {
    nrl_warning(NRL_CAT, "Inbound W3C trace parent invalid: parent id '%s'",
                str);
    nr_free(str);
    error = NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION;
    goto end;
  }
  nro_set_hash_string(traceparent_obj, "parent_id", str);
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "trace_flags");
  nro_set_hash_int(traceparent_obj, "trace_flags", strtol(str, NULL, 16));
  nr_free(str);

  nro_set_hash(obj, "traceparent", traceparent_obj);

end:
  nro_delete(traceparent_obj);
  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);

  return error;
}

/*
 * Purpose : Parse a W3C trace state header.
 *
 *           Refer to this specification for further details:
 *           https://w3c.github.io/trace-context/#tracestate-header
 *
 *           New Relic entries have the key '@nr', prefixed by the
 *           trusted account key.
 *
 *           New Relic entries consist of the following values,
 *           separated by dashes:
 *
 *           Name                  | Type           | Required
 *           ----------------------+----------------+---------
 *           version               | int            | yes
 *           parent_type           | int            | yes
 *           parent_account_id     | string         | yes
 *           parent_application_id | string         | yes
 *           span_id               | string         | no
 *           transaction_id        | string         | no
 *           sampled               | int            | no
 *           priority              | floating point | no
 *           timestamp             | int            | yes
 *
 *           For example:
 *           190@nr=0-0-709288-8599547-f85f42fd82a4cf1d-164d3b4b0d09cb05-1-0.789-1563574856827
 */
static const char* nr_distributed_trace_convert_w3c_headers_tracestate(
    nrobj_t* obj,
    const char* tracestate,
    const char* trusted_account_key NRUNUSED) {
  nrobj_t* vendors = NULL;
  char* tracing_vendors = NULL;
  char* headers_to_be_forwarded = NULL;
  nrobj_t* tracestate_obj = NULL;
  const char* error = NULL;
  char header_key[260] = {0};
  char header_value[260] = {0};
  const char* value;
  int index;
  char* regex_str = NULL;
  char* str = NULL;
  double priority;
  char* endptr = NULL;
  nr_regex_t* regex = NULL;
  nr_regex_substrings_t* ss = NULL;
  nrobj_t* parsed_vendor = NULL;

  if (NULL == tracestate || NULL == obj || NULL == trusted_account_key) {
    nrl_debug(NRL_CAT, "Inbound W3C trace state: NULL given");
    error = NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_NONRENTRY;
    goto end;
  }

  /*
   * Split the trace state header into key-value pairs.
   */
  snprintf(header_key, sizeof(header_key), "%s@nr=", trusted_account_key);

  vendors = nr_strsplit(tracestate, ",", 0);

  if (nro_getsize(vendors) == 0) {
    nrl_debug(NRL_CAT, "Inbound W3C trace state: no vendor strings");
    error = NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_NONRENTRY;
    goto end;
  }

  /*
   * Separate the relevant New Relic key-value pair from others.
   */
  for (index = 1; index <= nro_getsize(vendors); index++) {
    value = nro_get_array_string(vendors, index, NULL);

    if (0 == nr_strncmp(value, header_key, nr_strlen(header_key))) {
      nr_strlcpy(header_value, value, sizeof(header_value));
    } else {
      /*
       * Keep the other raw tracestate headers
       */
      headers_to_be_forwarded
          = nr_str_append(headers_to_be_forwarded, value, ",");

      /*
       * Keep the other tracing vendors
       */
      parsed_vendor = nr_strsplit(value, "=", 0);
      tracing_vendors = nr_str_append(
          tracing_vendors, nro_get_array_string(parsed_vendor, 1, NULL), ",");
      nro_delete(parsed_vendor);
    }
  }

  if (NULL != tracing_vendors) {
    nrl_debug(NRL_CAT, "Inbound W3C trace state: found %s other vendors",
              tracing_vendors);
    nro_set_hash_string(obj, "tracingVendors", tracing_vendors);
    nro_set_hash_string(obj, "rawTracingVendors", headers_to_be_forwarded);
  }

  if (0 == nr_strlen(header_value)) {
    nrl_debug(NRL_CAT, "Inbound W3C trace state: no NR entry");
    error = NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_NONRENTRY;
    goto end;
  } else {
    nrl_debug(NRL_CAT, "Inbound W3C trace state: found NR entry '%s'",
              header_value);
  }

  /*
   * Parse relevant New Relic key-value pair
   */
  regex_str = nr_formatf(
      "^"
      "%s"
      "(?P<version>[0-9]+)-"
      "(?P<parent_type>[0-9]+)-"
      "(?P<parent_account_id>[0-9a-zA-Z]+)-"
      "(?P<parent_application_id>[0-9a-zA-Z]+)-"
      "(?P<span_id>[0-9a-zA-Z]*)-"
      "(?P<transaction_id>[0-9a-zA-Z]*)-"
      "(?P<sampled>[0-9]*)-"
      "(?P<priority>[0-9.]*)-"
      "(?P<timestamp>[0-9]+)",
      header_key);

  regex = nr_regex_create(regex_str, 0, 0);

  ss = nr_regex_match_capture(regex, header_value, strlen(header_value));

  if (NULL == ss) {
    nrl_warning(NRL_CAT,
                "Inbound W3C trace state invalid: cannot parse NR entry '%s'",
                header_value);
    error = NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_INVALIDNRENTRY;
    goto end;
  }

  tracestate_obj = nro_new_hash();

  str = nr_regex_substrings_get_named(ss, "version");
  nro_set_hash_int(tracestate_obj, "version", strtol(str, NULL, 10));
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "parent_type");
  nro_set_hash_int(tracestate_obj, "parent_type", strtol(str, NULL, 10));
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "parent_account_id");
  nro_set_hash_string(tracestate_obj, "parent_account_id", str);
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "parent_application_id");
  nro_set_hash_string(tracestate_obj, "parent_application_id", str);
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "span_id");
  if (nr_strlen(str)) {
    nro_set_hash_string(tracestate_obj, "span_id", str);
  }
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "transaction_id");
  if (nr_strlen(str)) {
    nro_set_hash_string(tracestate_obj, "transaction_id", str);
  }
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "sampled");
  if (nr_strlen(str)) {
    nro_set_hash_int(tracestate_obj, "sampled", strtol(str, NULL, 10));
  }
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "priority");
  if (nr_strlen(str)) {
    endptr = NULL;
    priority = strtod(str, &endptr);
    if (endptr && *endptr != '\0') {
      /* According to the specification, an invalid priority value
       * should be treated as though it were omitted. */
      nrl_warning(NRL_CAT, "Inbound W3C trace state invalid: priority '%s'",
                  str);
    } else {
      nro_set_hash_double(tracestate_obj, "priority", priority);
    }
  }
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "timestamp");
  nro_set_hash_long(tracestate_obj, "timestamp", strtoull(str, NULL, 10));
  nr_free(str);

  nro_set_hash(obj, "tracestate", tracestate_obj);

end:
  nro_delete(tracestate_obj);
  nr_free(headers_to_be_forwarded);
  nr_free(tracing_vendors);
  nro_delete(vendors);
  nr_free(regex_str);
  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);

  return error;
}

nrobj_t* nr_distributed_trace_convert_w3c_headers_to_object(
    const char* traceparent,
    const char* tracestate,
    const char* trusted_account_key,
    const char** error) {
  nrobj_t* obj = NULL;
  const char* error_metric = NULL;

  obj = nro_new_hash();

  /*
   * Step 1 : Parse the trace parent header.
   */
  nrl_debug(NRL_CAT, "Inbound W3C trace parent: parsing '%s'", traceparent);

  error_metric
      = nr_distributed_trace_convert_w3c_headers_traceparent(obj, traceparent);
  if (error_metric) {
    nro_delete(obj);
    goto end;
  }

  /*
   * Step 2 : Parse the trace state header.
   */
  nrl_debug(NRL_CAT, "Inbound W3C trace state: parsing '%s'", tracestate);

  error_metric = nr_distributed_trace_convert_w3c_headers_tracestate(
      obj, tracestate, trusted_account_key);
  if (error_metric) {
    goto end;
  }

end:
  if (error_metric && error) {
    *error = error_metric;
  }
  return obj;
}

char* nr_distributed_trace_create_w3c_tracestate_header(
    const nr_distributed_trace_t* dt,
    const char* span_id,
    const char* txn_id) {
  const char* trusted_account_key;
  const char* account_id;
  const char* app_id;
  char* trace_context_header = NULL;
  char* sampled = "0";
  nr_sampling_priority_t priority;
  char* priority_buf = NULL;

  if (nrunlikely(NULL == dt)) {
    return NULL;
  }

  trusted_account_key = nr_distributed_trace_get_trusted_key(dt);
  // Trusted account key is not optional.
  if (NULL == trusted_account_key) {
    nrl_debug(
        NRL_CAT,
        "Could not create trace state header missing trusted account key");
    return NULL;
  }

  account_id = nr_distributed_trace_get_account_id(dt);
  // Account ID is not optional.
  if (NULL == account_id) {
    nrl_debug(NRL_CAT,
              "Could not create trace state header missing account id");
    return NULL;
  }

  app_id = nr_distributed_trace_get_app_id(dt);
  // App ID is not optional.
  if (NULL == app_id) {
    nrl_debug(NRL_CAT, "Could not create trace state header missing app id");
    return NULL;
  }

  if (nr_distributed_trace_is_sampled(dt)) {
    sampled = "1";
  }

  priority = nr_distributed_trace_get_priority(dt);

  priority_buf = nr_priority_double_to_str(priority);
  if (NULL == priority_buf) {
    nrl_verbosedebug(NRL_CAT, "Failed to allocate priority buffer");
  }

  trace_context_header = nr_formatf(
      "%s@nr=0-0-%s-%s-%s-%s-%s-%s-%" PRId64, trusted_account_key, account_id,
      app_id, NRBLANKSTR(span_id), NRBLANKSTR(txn_id), sampled,
      NRBLANKSTR(priority_buf), (nr_get_time() / NR_TIME_DIVISOR_MS));

  nr_free(priority_buf);

  return trace_context_header;
}

char* nr_distributed_trace_create_w3c_traceparent_header(const char* trace_id,
                                                         const char* span_id,
                                                         bool sampled) {
  char* trace_parent_header = NULL;
  char* flags = "00";
  char formatted_trace_id[33];
  char* tmp = NULL;
  int padding = 0;

  if (nrunlikely(NULL == trace_id || NULL == span_id)) {
    return NULL;
  }

  /*
   * The trace_id for a traceparent header is required to be 32 characters
   * long and lowercase. A trace_id is less than that will be left padded with
   * 0's.
   */
  tmp = nr_string_to_lowercase(trace_id);
  padding = NR_TRACE_ID_SIZE - nr_strlen(tmp);
  if (padding > 0) {
    snprintf(formatted_trace_id, sizeof(formatted_trace_id), "%0*d%s", padding,
             0, tmp);
  } else {
    snprintf(formatted_trace_id, sizeof(formatted_trace_id), "%s", tmp);
  }

  /*
   * The flags field is 2 digit hex. At time time of writing this we only
   * use sampled. If we add functionality for more flags this logic will
   * need to change. Since we only have one value we aren't doing any bit
   * masking to keep it readable.
   */
  if (sampled) {
    flags = "01";
  }

  // Version 00
  trace_parent_header
      = nr_formatf("00-%s-%s-%s", formatted_trace_id, span_id, flags);

  nr_free(tmp);

  return trace_parent_header;
}
