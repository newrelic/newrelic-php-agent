/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SPAN_EVENT_H
#define NR_SPAN_EVENT_H

#include "util_buffer.h"
#include "util_object.h"
#include "util_time.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct _nr_span_event_t nr_span_event_t;

/*
 * The categories a span may fall into.
 */
typedef enum {
  NR_SPAN_GENERIC,
  NR_SPAN_HTTP,
  NR_SPAN_DATASTORE,
  NR_SPAN_MESSAGE
} nr_span_category_t;

/*
 * The spankinds a span may fall into.
 * This is set according to:
 * 1) guidelines in agent-specs which state datastore and http spans set
 * span.kind to client and further states that generic span.kind is unset
 *
 * 2) for message spans follow guidance here:
 * https://opentelemetry.io/docs/specs/semconv/messaging/messaging-spans/
 * which states that span.kind is
 * a) producer when the operation type is create or send(if the context is
 * create) b) client when the operation type is create or send(if the context is
 * NOT create) c) consumer when the operation type is process
 */
typedef enum {
  NR_SPAN_PRODUCER,
  NR_SPAN_CLIENT,
  NR_SPAN_CONSUMER,
  NR_SPAN_NO_SPANKIND
} nr_span_spankind_t;

/*
 * Fields that can be set on datastore spans.
 */
typedef enum {
  NR_SPAN_DATASTORE_COMPONENT,
  NR_SPAN_DATASTORE_DB_STATEMENT,
  NR_SPAN_DATASTORE_DB_INSTANCE,
  NR_SPAN_DATASTORE_PEER_ADDRESS,
  NR_SPAN_DATASTORE_PEER_HOSTNAME
} nr_span_event_datastore_member_t;

/*
 * Fields that can be set on external (HTTP) spans.
 */
typedef enum {
  NR_SPAN_EXTERNAL_COMPONENT,
  NR_SPAN_EXTERNAL_URL,
  NR_SPAN_EXTERNAL_METHOD
} nr_span_event_external_member_t;

/*
 * Fields that can be set on message spans.
 */
typedef enum {
  NR_SPAN_MESSAGE_DESTINATION_NAME,
  NR_SPAN_MESSAGE_CLOUD_REGION,
  NR_SPAN_MESSAGE_CLOUD_ACCOUNT_ID,
  NR_SPAN_MESSAGE_MESSAGING_SYSTEM,
  NR_SPAN_MESSAGE_CLOUD_RESOURCE_ID,
  NR_SPAN_MESSAGE_SERVER_ADDRESS
} nr_span_event_message_member_t;

/*
 * The parent attributes that can be set on service entry spans.
 * parent.transportDuration is set in
 * nr_span_event_set_parent_transport_duration()
 */
typedef enum {
  NR_SPAN_PARENT_TYPE,
  NR_SPAN_PARENT_APP,
  NR_SPAN_PARENT_ACCOUNT,
  NR_SPAN_PARENT_TRANSPORT_TYPE
} nr_span_event_parent_attributes_t;

/*
 * Purpose : Create a new span event.
 *
 * Returns : A span event, ready to receive span event attributes and
 *           intrinsics. The span event must be destroyed with
 *           nr_span_event_destroy().
 */
extern nr_span_event_t* nr_span_event_create(void);

/*
 * Purpose : Destroys/frees structs created via nr_span_event_create.
 *
 * Params  : A pointer to the pointer that points at the allocated
 *           nr_span_event_t (created with nr_span_event_create).
 *
 * Returns : Nothing.
 */
extern void nr_span_event_destroy(nr_span_event_t** ptr);

/*
 * Purpose : Output New Relic format JSON for the given span event.
 *
 * Params  : 1. The span event.
 *
 * Returns : The JSON, which is owned by the caller, or NULL on error.
 */
extern char* nr_span_event_to_json(const nr_span_event_t* event);

/*
 * Purpose : Append New Relic format JSON for a span event to a buffer.
 *
 * Params  : 1. The span event.
 *           2. The buffer to append to.
 *
 * Returns : True on success; false on error.
 */
extern bool nr_span_event_to_json_buffer(const nr_span_event_t* event,
                                         nrbuf_t* buf);

/*
 * Purpose : Set the various fields of the span events.
 *
 * Params : 1. The span event.
 *          2. The field to be set.
 *
 * Returns : Nothing.
 */
extern void nr_span_event_set_guid(nr_span_event_t* event, const char* guid);
extern void nr_span_event_set_parent_id(nr_span_event_t* event,
                                        const char* parent_id);
extern void nr_span_event_set_trace_id(nr_span_event_t* event,
                                       const char* trace_id);
extern void nr_span_event_set_transaction_id(nr_span_event_t* event,
                                             const char* transaction_id);
extern void nr_span_event_set_name(nr_span_event_t* event, const char* name);
extern void nr_span_event_set_transaction_name(nr_span_event_t* event,
                                               const char* transaction_name);
extern void nr_span_event_set_category(nr_span_event_t* event,
                                       nr_span_category_t category);
extern void nr_span_event_set_spankind(nr_span_event_t* event,
                                       nr_span_spankind_t category);
extern void nr_span_event_set_timestamp(nr_span_event_t* event, nrtime_t time);
extern void nr_span_event_set_duration(nr_span_event_t* event,
                                       nrtime_t duration);
extern void nr_span_event_set_priority(nr_span_event_t* event, double priority);
extern void nr_span_event_set_sampled(nr_span_event_t* event, bool sampled);
extern void nr_span_event_set_entry_point(nr_span_event_t* event,
                                          bool entry_point);
extern void nr_span_event_set_tracing_vendors(nr_span_event_t* event,
                                              const char* tracing_vendors);
extern void nr_span_event_set_trusted_parent_id(nr_span_event_t* event,
                                                const char* trusted_parent_id);
extern void nr_span_event_set_parent_transport_duration(
    nr_span_event_t* event,
    nrtime_t transport_duration);
extern void nr_span_event_set_error_message(nr_span_event_t* event,
                                            const char* error_message);
extern void nr_span_event_set_error_class(nr_span_event_t* event,
                                          const char* error_class);

/*
 * Purpose : Set datastore fields.
 *
 * Params : 1. The Span Event that you would like changed
 *          2. The member of the struct you would like to update, using the
 *          nr_span_event_datastore_member_t enum.
 *          3. The string value would like stored in the specified field
 *
 * Returns : Nothing.
 */
extern void nr_span_event_set_datastore(nr_span_event_t* event,
                                        nr_span_event_datastore_member_t member,
                                        const char* new_value);

/*
 * Purpose : Set an external attribute.
 *
 * Params : 1. The target Span Event that should be changed.
 *          2. The external attribute to be set.
 *          3. The string value that the field will be after the function has
 *             executed.
 */
extern void nr_span_event_set_external(nr_span_event_t* event,
                                       nr_span_event_external_member_t member,
                                       const char* new_value);

/*
 * Purpose : Set the external http.statusCode attribute.
 *
 * Params : 1. The target Span Event that should be changed.
 *          2. The int value that the field will be after the function has
 *             executed.
 */
extern void nr_span_event_set_external_status(nr_span_event_t* event,
                                              const uint64_t status);

/*
 * Purpose : Set a message attribute.
 *
 * Params : 1. The target Span Event that should be changed.
 *          2. The message attribute to be set.
 *          3. The string value that the field will be after the function has
 *             executed.
 */
extern void nr_span_event_set_message(nr_span_event_t* event,
                                      nr_span_event_message_member_t member,
                                      const char* new_value);

/*
 * Purpose : Set a user attribute.
 *
 * Params : 1. The pointer to the span event.
 *          2. The name of the attribute to be added.
 *          3. The value of the attribute to be added.
 */
extern void nr_span_event_set_attribute_user(nr_span_event_t* event,
                                             const char* name,
                                             const nrobj_t* value);

/*
 * Purpose : Set a parent attribute.
 *
 * Params : 1. The pointer to the span event.
 *          2. The parent attribute to be set.
 *          3. The value to be set.
 */
extern void nr_span_event_set_parent_attribute(
    nr_span_event_t* event,
    nr_span_event_parent_attributes_t member,
    const char* value);

/*
 * Purpose : Set an agent attribute.
 *
 * Params : 1. The pointer to the span event.
 *          2. The name of the attribute to be added.
 *          3. The value of the attribute to be added.
 */
extern void nr_span_event_set_attribute_agent(nr_span_event_t* event,
                                              const char* name,
                                              const nrobj_t* value);
#endif /* NR_SPAN_EVENT_H */
