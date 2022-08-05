/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_EVENTS_HDR
#define NR_LOG_EVENTS_HDR

#include "nr_analytics_events.h"
#include "nr_log_event.h"
#include "util_random.h"

/*
 * Purpose : Convert a log event to an analytic event and add it to
 *           an event pool.
 * 
 * Returns : true if sampling occured (see Notes below)
 *           false otherwise
 *
 * Notes   : This function is not guaranteed to add an event: Once the events
 *           data structure is full, this event may replace an existing event
 *           based upon a sampling algorithm.
 */
extern bool nr_log_events_add_event(nr_analytics_events_t* events,
                                    const nr_log_event_t* event,
                                    nr_random_t* rnd);

/* Function aliases redirecting to underlying analytics events implementation */
NRINLINE nr_analytics_events_t* nr_log_events_create(int max_events) {
  return nr_analytics_events_create(max_events);
}
NRINLINE void nr_log_events_destroy(nr_analytics_events_t** events) {
  return nr_analytics_events_destroy(events);
}
NRINLINE int nr_log_events_number_seen(const nr_analytics_events_t* events) {
  return nr_analytics_events_number_seen(events);
}
NRINLINE int nr_log_events_number_saved(const nr_analytics_events_t* events) {
  return nr_analytics_events_number_saved(events);
}
NRINLINE const char* nr_log_events_get_event_json(nr_analytics_events_t* events,
                                                  int i) {
  return nr_analytics_events_get_event_json(events, i);
}
#endif