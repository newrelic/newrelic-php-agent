/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_span_event.h"
#include "nr_span_event_private.h"
#include "util_memory.h"
#include "util_time.h"

nr_span_event_t* nr_span_event_create() {
  nr_span_event_t* se;

  se = (nr_span_event_t*)nr_malloc(sizeof(nr_span_event_t));

  se->trace_id = NULL;
  se->intrinsics = nro_new_hash();
  se->agent_attributes = nro_new_hash();
  se->user_attributes = nro_new_hash();

  nro_set_hash_string(se->intrinsics, "category", "generic");
  nro_set_hash_string(se->intrinsics, "type", "Span");

  return se;
}

void nr_span_event_destroy(nr_span_event_t** ptr) {
  nr_span_event_t* event = NULL;

  if ((NULL == ptr) || (NULL == *ptr)) {
    return;
  }

  event = *ptr;
  nr_free(event->trace_id);
  nro_delete(event->intrinsics);
  nro_delete(event->agent_attributes);
  nro_delete(event->user_attributes);

  nr_realfree((void**)ptr);
}

char* nr_span_event_to_json(const nr_span_event_t* event) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  char* json = NULL;

  if (!nr_span_event_to_json_buffer(event, buf)) {
    goto end;
  }

  nr_buffer_add(buf, NR_PSTR("\0"));
  json = nr_strdup(nr_buffer_cptr(buf));

end:
  nr_buffer_destroy(&buf);
  return json;
}

bool nr_span_event_to_json_buffer(const nr_span_event_t* event, nrbuf_t* buf) {
  if (NULL == event || NULL == buf) {
    return false;
  }

  // We'll build the JSON manually to avoid copying the hashes into a new
  // nrobj_t array, which is expensive and pointless given it's a fixed length
  // array.
  nr_buffer_add(buf, NR_PSTR("["));
  nro_to_json_buffer(event->intrinsics, buf);
  nr_buffer_add(buf, NR_PSTR(","));
  nro_to_json_buffer(event->user_attributes, buf);
  nr_buffer_add(buf, NR_PSTR(","));
  nro_to_json_buffer(event->agent_attributes, buf);
  nr_buffer_add(buf, NR_PSTR("]"));

  return true;
}

void nr_span_event_set_guid(nr_span_event_t* event, const char* guid) {
  if (NULL == event || NULL == guid) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "guid", guid);
}

void nr_span_event_set_parent_id(nr_span_event_t* event,
                                 const char* parent_id) {
  if (NULL == event || NULL == parent_id) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "parentId", parent_id);
}

void nr_span_event_set_trace_id(nr_span_event_t* event, const char* trace_id) {
  if (NULL == event) {
    return;
  }

  nr_free(event->trace_id);
  if (trace_id) {
    nro_set_hash_string(event->intrinsics, "traceId", trace_id);
    event->trace_id = nr_strdup(trace_id);
  }
}

void nr_span_event_set_transaction_id(nr_span_event_t* event,
                                      const char* transaction_id) {
  if (NULL == event || NULL == transaction_id) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "transactionId", transaction_id);
}

void nr_span_event_set_name(nr_span_event_t* event, const char* name) {
  if (NULL == event || NULL == name) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "name", name);
}

void nr_span_event_set_transaction_name(nr_span_event_t* event,
                                        const char* transaction_name) {
  if (NULL == event || NULL == transaction_name) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "transaction.name", transaction_name);
}

void nr_span_event_set_category(nr_span_event_t* event,
                                nr_span_category_t category) {
  if (NULL == event) {
    return;
  }

  switch (category) {
    case NR_SPAN_DATASTORE:
      nro_set_hash_string(event->intrinsics, "category", "datastore");
      nr_span_event_set_spankind(event, NR_SPAN_CLIENT);
      break;

    case NR_SPAN_GENERIC:
      nro_set_hash_string(event->intrinsics, "category", "generic");
      nr_span_event_set_spankind(event, NR_SPAN_NO_SPANKIND) break;

    case NR_SPAN_HTTP:
      nro_set_hash_string(event->intrinsics, "category", "http");
      nr_span_event_set_spankind(event, NR_SPAN_CLIENT);
      break;

    case NR_SPAN_MESSAGE:
      nro_set_hash_string(event->intrinsics, "category", "message");
      /* give it a default value in case we exit before spankind is set*/
      nr_span_event_set_spankind(event, NR_SPAN_NO_SPANKIND);
      break;
  }
}

void nr_span_event_set_spankind(nr_span_event_t* event,
                                nr_span_spankind_t spankind) {
  if (NULL == event) {
    return;
  }

  switch (spankind) {
    case NR_SPAN_PRODUCER:
      nro_set_hash_string(event->intrinsics, "span.kind", "producer");
      break;
    case NR_SPAN_CLIENT:
      nro_set_hash_string(event->intrinsics, "span.kind", "client");
      break;
    case NR_SPAN_CONSUMER:
      nro_set_hash_string(event->intrinsics, "span.kind", "consumer");
      break;
    case NR_SPAN_NO_SPANKIND:
      if (nro_get_hash_value(event->intrinsics, "span.kind", NULL)) {
        nro_set_hash_none(event->intrinsics, "span.kind");
      }
      break;
  }
}

void nr_span_event_set_timestamp(nr_span_event_t* event, nrtime_t time) {
  if (NULL == event) {
    return;
  }

  nro_set_hash_ulong(event->intrinsics, "timestamp", time / NR_TIME_DIVISOR_MS);
}

void nr_span_event_set_duration(nr_span_event_t* event, nrtime_t duration) {
  if (NULL == event) {
    return;
  }

  nro_set_hash_double(event->intrinsics, "duration",
                      duration / NR_TIME_DIVISOR_D);
}

void nr_span_event_set_priority(nr_span_event_t* event, double priority) {
  if (NULL == event) {
    return;
  }

  nro_set_hash_double(event->intrinsics, "priority", priority);
}

void nr_span_event_set_sampled(nr_span_event_t* event, bool sampled) {
  if (NULL == event) {
    return;
  }

  nro_set_hash_boolean(event->intrinsics, "sampled", sampled);
}

void nr_span_event_set_entry_point(nr_span_event_t* event, bool entry_point) {
  if (NULL == event) {
    return;
  }

  if (entry_point) {
    nro_set_hash_boolean(event->intrinsics, "nr.entryPoint", entry_point);
  }
}

void nr_span_event_set_tracing_vendors(nr_span_event_t* event,
                                       const char* tracing_vendors) {
  if (NULL == event || NULL == tracing_vendors) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "tracingVendors", tracing_vendors);
}

void nr_span_event_set_trusted_parent_id(nr_span_event_t* event,
                                         const char* trusted_parent_id) {
  if (NULL == event || NULL == trusted_parent_id) {
    return;
  }

  nro_set_hash_string(event->intrinsics, "trustedParentId", trusted_parent_id);
}

void nr_span_event_set_error_message(nr_span_event_t* event,
                                     const char* error_message) {
  if (NULL == event || NULL == error_message) {
    return;
  }

  nro_set_hash_string(event->agent_attributes, "error.message", error_message);
}

void nr_span_event_set_error_class(nr_span_event_t* event,
                                   const char* error_class) {
  if (NULL == event || NULL == error_class) {
    return;
  }

  nro_set_hash_string(event->agent_attributes, "error.class", error_class);
}

void nr_span_event_set_parent_attribute(
    nr_span_event_t* event,
    nr_span_event_parent_attributes_t member,
    const char* value) {
  if (NULL == event || NULL == value) {
    return;
  }

  switch (member) {
    case NR_SPAN_PARENT_TYPE:
      nro_set_hash_string(event->agent_attributes, "parent.type", value);
      break;
    case NR_SPAN_PARENT_APP:
      nro_set_hash_string(event->agent_attributes, "parent.app", value);
      break;
    case NR_SPAN_PARENT_ACCOUNT:
      nro_set_hash_string(event->agent_attributes, "parent.account", value);
      break;
    case NR_SPAN_PARENT_TRANSPORT_TYPE:
      nro_set_hash_string(event->agent_attributes, "parent.transportType",
                          value);
      break;
  }
}

void nr_span_event_set_parent_transport_duration(nr_span_event_t* event,
                                                 nrtime_t transport_duration) {
  if (NULL == event) {
    return;
  }

  nro_set_hash_double(event->agent_attributes, "parent.transportDuration",
                      transport_duration / NR_TIME_DIVISOR);
}

void nr_span_event_set_datastore(nr_span_event_t* event,
                                 nr_span_event_datastore_member_t member,
                                 const char* new_value) {
  if (NULL == event || NULL == new_value) {
    return;
  }

  switch (member) {
    case NR_SPAN_DATASTORE_COMPONENT:
      nro_set_hash_string(event->intrinsics, "component", new_value);
      break;
    case NR_SPAN_DATASTORE_DB_STATEMENT:
      nro_set_hash_string(event->agent_attributes, "db.statement", new_value);
      break;
    case NR_SPAN_DATASTORE_DB_INSTANCE:
      nro_set_hash_string(event->agent_attributes, "db.instance", new_value);
      break;
    case NR_SPAN_DATASTORE_PEER_ADDRESS:
      nro_set_hash_string(event->agent_attributes, "peer.address", new_value);
      break;
    case NR_SPAN_DATASTORE_PEER_HOSTNAME:
      nro_set_hash_string(event->agent_attributes, "peer.hostname", new_value);
      break;
  }
  return;
}

void nr_span_event_set_external(nr_span_event_t* event,
                                nr_span_event_external_member_t member,
                                const char* new_value) {
  if (NULL == event || NULL == new_value) {
    return;
  }

  switch (member) {
    case NR_SPAN_EXTERNAL_URL:
      nro_set_hash_string(event->agent_attributes, "http.url", new_value);
      break;
    case NR_SPAN_EXTERNAL_METHOD:
      nro_set_hash_string(event->agent_attributes, "http.method", new_value);
      break;
    case NR_SPAN_EXTERNAL_COMPONENT:
      nro_set_hash_string(event->intrinsics, "component", new_value);
      break;
  }
}

void nr_span_event_set_external_status(nr_span_event_t* event,
                                       const uint64_t status) {
  if (NULL == event) {
    return;
  }

  nro_set_hash_ulong(event->agent_attributes, "http.statusCode", status);
}

void nr_span_event_set_message(nr_span_event_t* event,
                               nr_span_event_message_member_t member,
                               const char* new_value) {
  if (NULL == event || NULL == new_value) {
    return;
  }

  switch (member) {
    case NR_SPAN_MESSAGE_DESTINATION_NAME:
      nro_set_hash_string(event->agent_attributes, "messaging.destination.name",
                          new_value);
      break;
    case NR_SPAN_MESSAGE_CLOUD_REGION:
      nro_set_hash_string(event->agent_attributes, "cloud.region", new_value);
      break;
    case NR_SPAN_MESSAGE_CLOUD_ACCOUNT_ID:
      nro_set_hash_string(event->intrinsics, "cloud.account.id", new_value);
      break;
    case NR_SPAN_MESSAGE_MESSAGING_SYSTEM:
      nro_set_hash_string(event->agent_attributes, "messaging.system",
                          new_value);
      break;
    case NR_SPAN_MESSAGE_CLOUD_RESOURCE_ID:
      nro_set_hash_string(event->agent_attributes, "cloud.resource_id",
                          new_value);
      break;
    case NR_SPAN_MESSAGE_SERVER_ADDRESS:
      nro_set_hash_string(event->intrinsics, "server.address", new_value);
      break;
  }
}

/*
 * Getters.
 *
 * We only use these for unit tests.
 *
 * These are generated using macros, which is ugly and horrible, but saves a
 * bunch of boilerplate.
 */
#define SPAN_EVENT_GETTER(name, type, obj, getter, field, default_value) \
  type name(const nr_span_event_t* event) {                              \
    nr_status_t err = NR_FAILURE;                                        \
    type rv = default_value;                                             \
                                                                         \
    if (NULL == event) {                                                 \
      return rv;                                                         \
    }                                                                    \
                                                                         \
    rv = getter(event->obj, field, &err);                                \
    if (NR_SUCCESS != err) {                                             \
      return default_value;                                              \
    }                                                                    \
    return rv;                                                           \
  }

#define SPAN_EVENT_GETTER_BOOL(name, obj, field) \
  SPAN_EVENT_GETTER(name, bool, obj, nro_get_hash_boolean, field, false)

#define SPAN_EVENT_GETTER_DOUBLE(name, obj, field) \
  SPAN_EVENT_GETTER(name, double, obj, nro_get_hash_double, field, 0.0)

#define SPAN_EVENT_GETTER_STRING(name, obj, field) \
  SPAN_EVENT_GETTER(name, const char*, obj, nro_get_hash_string, field, NULL)

#define SPAN_EVENT_GETTER_TIME(name, obj, field) \
  SPAN_EVENT_GETTER(name, nrtime_t, obj, nro_get_hash_ulong, field, 0)

#define SPAN_EVENT_GETTER_UINT(name, obj, field) \
  SPAN_EVENT_GETTER(name, uint64_t, obj, nro_get_hash_ulong, field, 0)

SPAN_EVENT_GETTER_STRING(nr_span_event_get_guid, intrinsics, "guid")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_parent_id, intrinsics, "parentId")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_trace_id, intrinsics, "traceId")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_transaction_id,
                         intrinsics,
                         "transactionId")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_name, intrinsics, "name")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_transaction_name,
                         intrinsics,
                         "transaction.name")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_category, intrinsics, "category")
SPAN_EVENT_GETTER_TIME(nr_span_event_get_timestamp, intrinsics, "timestamp")
SPAN_EVENT_GETTER_DOUBLE(nr_span_event_get_duration, intrinsics, "duration")
SPAN_EVENT_GETTER_DOUBLE(nr_span_event_get_priority, intrinsics, "priority")
SPAN_EVENT_GETTER_BOOL(nr_span_event_is_sampled, intrinsics, "sampled")
SPAN_EVENT_GETTER_BOOL(nr_span_event_is_entry_point,
                       intrinsics,
                       "nr.entryPoint")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_tracing_vendors,
                         intrinsics,
                         "tracingVendors")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_trusted_parent_id,
                         intrinsics,
                         "trustedParentId")
SPAN_EVENT_GETTER_DOUBLE(nr_span_event_get_parent_transport_duration,
                         agent_attributes,
                         "parent.transportDuration")
SPAN_EVENT_GETTER_UINT(nr_span_event_get_external_status,
                       agent_attributes,
                       "http.statusCode")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_error_message,
                         agent_attributes,
                         "error.message")
SPAN_EVENT_GETTER_STRING(nr_span_event_get_error_class,
                         agent_attributes,
                         "error.class")

const char* nr_span_event_get_parent_attribute(
    const nr_span_event_t* event,
    nr_span_event_parent_attributes_t member) {
  if (NULL == event) {
    return NULL;
  }

  switch (member) {
    case NR_SPAN_PARENT_TYPE:
      return nro_get_hash_string(event->agent_attributes, "parent.type", NULL);
    case NR_SPAN_PARENT_APP:
      return nro_get_hash_string(event->agent_attributes, "parent.app", NULL);
    case NR_SPAN_PARENT_ACCOUNT:
      return nro_get_hash_string(event->agent_attributes, "parent.account",
                                 NULL);
    case NR_SPAN_PARENT_TRANSPORT_TYPE:
      return nro_get_hash_string(event->agent_attributes,
                                 "parent.transportType", NULL);
  }
  return NULL;
}

const char* nr_span_event_get_datastore(
    const nr_span_event_t* event,
    nr_span_event_datastore_member_t member) {
  if (NULL == event) {
    return NULL;
  }

  switch (member) {
    case NR_SPAN_DATASTORE_COMPONENT:
      return nro_get_hash_string(event->intrinsics, "component", NULL);
    case NR_SPAN_DATASTORE_DB_STATEMENT:
      return nro_get_hash_string(event->agent_attributes, "db.statement", NULL);
    case NR_SPAN_DATASTORE_DB_INSTANCE:
      return nro_get_hash_string(event->agent_attributes, "db.instance", NULL);
    case NR_SPAN_DATASTORE_PEER_ADDRESS:
      return nro_get_hash_string(event->agent_attributes, "peer.address", NULL);
    case NR_SPAN_DATASTORE_PEER_HOSTNAME:
      return nro_get_hash_string(event->agent_attributes, "peer.hostname",
                                 NULL);
  }
  return NULL;
}

const char* nr_span_event_get_external(const nr_span_event_t* event,
                                       nr_span_event_external_member_t member) {
  if (NULL == event) {
    return NULL;
  }

  switch (member) {
    case NR_SPAN_EXTERNAL_URL:
      return nro_get_hash_string(event->agent_attributes, "http.url", NULL);
    case NR_SPAN_EXTERNAL_METHOD:
      return nro_get_hash_string(event->agent_attributes, "http.method", NULL);
    case NR_SPAN_EXTERNAL_COMPONENT:
      return nro_get_hash_string(event->intrinsics, "component", NULL);
  }
  return NULL;
}

void nr_span_event_set_attribute_user(nr_span_event_t* event,
                                      const char* name,
                                      const nrobj_t* value) {
  if (NULL == event || NULL == name || NULL == value) {
    return;
  }

  nro_set_hash(event->user_attributes, name, value);
}

void nr_span_event_set_attribute_agent(nr_span_event_t* event,
                                       const char* name,
                                       const nrobj_t* value) {
  if (NULL == event || NULL == name || NULL == value) {
    return;
  }

  nro_set_hash(event->agent_attributes, name, value);
}
