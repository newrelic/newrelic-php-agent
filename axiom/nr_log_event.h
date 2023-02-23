/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_EVENT_HDR
#define NR_LOG_EVENT_HDR

#include "util_buffer.h"
#include "util_time.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct _nr_log_event_t nr_log_event_t;

/* Maximum length of message allowed (longer will be truncated) */
#define NR_MAX_LOG_MESSAGE_LEN 32768

/*
 * Purpose : Create a new log event.
 *
 * Returns : A log event, ready to receive log event attributes and
 *           intrinsics. The log event must be destroyed with
 *           nr_log_event_destroy().
 */
extern nr_log_event_t* nr_log_event_create(void);

/*
 * Purpose : Destroys/frees structs created via nr_log_event_create.
 *
 * Params  : A pointer to the pointer that points at the allocated
 *           nr_log_event_t (created with nr_log_event_create).
 *
 * Returns : Nothing.
 */
extern void nr_log_event_destroy(nr_log_event_t** ptr);

/*
 * Purpose : Output New Relic format JSON for the given log event.
 *
 * Params  : 1. The log event.
 *
 * Returns : The JSON, which is owned by the caller, or NULL on error.
 */
extern char* nr_log_event_to_json(const nr_log_event_t* event);

/*
 * Purpose : Append New Relic format JSON for a log event to a buffer.
 *
 * Params  : 1. The log event.
 *           2. The buffer to append to.
 *
 * Returns : True on success; false on error.
 */
extern bool nr_log_event_to_json_buffer(const nr_log_event_t* event,
                                        nrbuf_t* buf);

/*
 * Purpose : Comparator function for use with priority sampling
 *
 * Params  : 1. First log event.
 *           2. Second log event to compare tofirst.
 *           3. Possible user data
 *
 * Returns : Compares events and returns -1, 0, or 1.
 *
 * Notes   : This function is used for the minmax heap to
 *           compare 2 log events.  The function considers
 *           the priority of the 2 events and if this is
 *           equal then the ages of the events are compared.
 */
int nr_log_event_wrapped_priority_comparator(const void* a,
                                             const void* b,
                                             void* userdata NRUNUSED);

/*
 * Purpose : Set the various fields of the log events.
 *
 * Params : 1. The log event.
 *          2. The field to be set.
 *
 * Returns : Nothing.
 *
 * Notes   : nr_log_event_set_message() truncates the message
 *           length to NR_MAX_LOG_MESSAGE_LEN.
 */
extern void nr_log_event_set_message(nr_log_event_t* event,
                                     const char* message);
extern void nr_log_event_set_log_level(nr_log_event_t* event,
                                       const char* log_level);
extern void nr_log_event_set_guid(nr_log_event_t* event, const char* guid);
extern void nr_log_event_set_trace_id(nr_log_event_t* event,
                                      const char* trace_id);
extern void nr_log_event_set_span_id(nr_log_event_t* event,
                                     const char* span_id);
extern void nr_log_event_set_entity_name(nr_log_event_t* event,
                                         const char* name);
extern void nr_log_event_set_hostname(nr_log_event_t* event, const char* name);
extern void nr_log_event_set_timestamp(nr_log_event_t* event, nrtime_t time);
extern void nr_log_event_set_priority(nr_log_event_t* event, int priority);

#endif /* NR_LOG_EVENT_HDR */
