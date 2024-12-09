/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SPAN_EVENT_PRIVATE_H
#define NR_SPAN_EVENT_PRIVATE_H

#include "nr_span_event.h"
#include "util_object.h"

struct _nr_span_event_t {
  char* trace_id;
  nrobj_t* intrinsics;
  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;
};

/*
 * Getters, used only for unit tests.
 */
extern const char* nr_span_event_get_guid(const nr_span_event_t* event);
extern const char* nr_span_event_get_parent_id(const nr_span_event_t* event);
extern const char* nr_span_event_get_trace_id(const nr_span_event_t* event);
extern const char* nr_span_event_get_transaction_id(
    const nr_span_event_t* event);
extern const char* nr_span_event_get_name(const nr_span_event_t* event);
extern const char* nr_span_event_get_transaction_name(
    const nr_span_event_t* event);
extern const char* nr_span_event_get_category(const nr_span_event_t* event);
extern const char* nr_span_event_get_spankind(const nr_span_event_t* event);
extern nrtime_t nr_span_event_get_timestamp(const nr_span_event_t* event);
extern double nr_span_event_get_duration(const nr_span_event_t* event);
extern double nr_span_event_get_priority(const nr_span_event_t* event);
extern bool nr_span_event_is_sampled(const nr_span_event_t* event);
extern bool nr_span_event_is_entry_point(const nr_span_event_t* event);
extern const char* nr_span_event_get_tracing_vendors(
    const nr_span_event_t* event);
extern const char* nr_span_event_get_trusted_parent_id(
    const nr_span_event_t* event);
extern const char* nr_span_event_get_datastore(
    const nr_span_event_t* event,
    nr_span_event_datastore_member_t member);
extern const char* nr_span_event_get_external(
    const nr_span_event_t* event,
    nr_span_event_external_member_t member);
extern uint64_t nr_span_event_get_external_status(const nr_span_event_t* event);
extern const char* nr_span_event_get_message(
    const nr_span_event_t* event,
    nr_span_event_message_member_t member);
extern const char* nr_span_event_get_error_message(
    const nr_span_event_t* event);
extern const char* nr_span_event_get_error_class(const nr_span_event_t* event);
extern double nr_span_event_get_parent_transport_duration(
    const nr_span_event_t* event);
extern const char* nr_span_event_get_parent_attribute(
    const nr_span_event_t* event,
    nr_span_event_parent_attributes_t member);

#endif /* NR_SPAN_EVENT_PRIVATE_H */
