/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_EVENTS_HDR
#define NR_LOG_EVENTS_HDR

//#include "nr_analytics_events.h"
#include "nr_log_event.h"
#include "util_random.h"

typedef struct _nr_log_events_t nr_log_events_t;

/*
 * Purpose : Add a log event to a log event pool.
 *
 * Returns : true if and only if sampling occured (see Notes below)
 *           false otherwise (sampling not occurred; event was not added
 *           because function was called with NULL arguments, which implies
 *           sampling not occurred).
 *
 * Notes   : This function is not guaranteed to add an event: Once the events
 *           data structure is full, this event may replace an existing event
 *           based upon a sampling algorithm.
 */
extern bool nr_log_events_add_event(nr_log_events_t* events,
                                    const nr_log_event_t* event);

extern nr_log_events_t* nr_log_events_create(int max_events);
extern void nr_log_events_destroy(nr_log_events_t** events);
extern int nr_log_events_max_events(const nr_log_events_t* events);
extern int nr_log_events_number_seen(const nr_log_events_t* events);
extern int nr_log_events_number_saved(const nr_log_events_t* events);
extern bool nr_log_events_is_sampling(nr_log_events_t* events);
// const char* nr_log_events_get_event_json(nr_log_events_t* events, int i);

#endif /* NR_LOG_EVENTS_HDR */