/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to store and format event data for
 * New Relic Insights.
 */
#ifndef NR_ANALYTICS_EVENTS_HDR
#define NR_ANALYTICS_EVENTS_HDR

#include <stdbool.h>

#include "util_object.h"
#include "util_random.h"

/*
 * This structure represents a single event.  One of these structures is
 * created per transaction.
 */
typedef struct _nr_analytics_event_t nr_analytics_event_t;

/*
 * This is a pool of events.  Each application's harvest structure holds
 * one of these.
 */
typedef struct _nr_analytics_events_t nr_analytics_events_t;

/*
 * Purpose : Create a new analytics event.
 *
 * Params  : 1. Normal fields such as 'type' and 'duration'.
 *           2. Attributes created by the user using an API call.
 *           3. Attributes created by the agent.
 *
 *           The values in these hashes should be strings, doubles
 *           and longs.
 */
nr_analytics_event_t* nr_analytics_event_create(const nrobj_t* builtin_fields,
                                                const nrobj_t* agent_attributes,
                                                const nrobj_t* user_attributes);

/*
 * Purpose : Destroy an analytics event, releasing all of its memory.
 *
 */
extern void nr_analytics_event_destroy(nr_analytics_event_t** event_ptr);

/*
 * Purpose : Create a data structure to hold analytics event data.
 *
 * Params  : 1. The total number of events that will be recorded.  After this
 *              maximum is reached, events will be saved/replaced using
 *              a sampling algorithm.
 *
 * Returns : A newly allocated events structure, or 0 on error.
 */
extern nr_analytics_events_t* nr_analytics_events_create(int max_events);

/*
 * Purpose : Create a data structure to hold analytics event data.
 *
 * Params  : 1. The total number of events that will be recorded (0 is allowd).
 *              After this maximum is reached, events will be saved/replaced
 *              using a sampling algorithm.
 *
 * Returns : A newly allocated events structure, or NULL on error.
 */
extern nr_analytics_events_t* nr_analytics_events_create_ex(int max_events);

/*
 * Purpose : Get the number of events that were attempted to be put in the
 *           structure using add_event.
 */
extern int nr_analytics_events_number_seen(const nr_analytics_events_t* events);

/*
 * Purpose : Get the number of events saved within the structure.
 */
extern int nr_analytics_events_number_saved(
    const nr_analytics_events_t* events);

/*
 * Purpose : Destroy an analytics event data structure, freeing all of its
 *           associated memory.
 */
extern void nr_analytics_events_destroy(nr_analytics_events_t** events_ptr);

/*
 * Purpose : Inform the outside world if events are being sampled when adding
             them to the event pool.
 */
extern bool nr_analytics_events_is_sampling(nr_analytics_events_t* events);

/*
 * Purpose : Inform the outside world about maximum number of events that can be
 *           stored.
 */
extern int nr_analytics_events_max_events(const nr_analytics_events_t* events);

/*
 * Purpose : Add an event to an event pool.
 *
 * Notes   : This function is not guaranteed to add an event: Once the events
 *           data structure is full, this event may replace an existing event
 *           based upon a sampling algorithm.
 */
extern void nr_analytics_events_add_event(nr_analytics_events_t* events,
                                          const nr_analytics_event_t* event,
                                          nr_random_t* rnd);

/*
 * Purpose : Get event JSON from an event pool.
 */
extern const char* nr_analytics_events_get_event_json(
    nr_analytics_events_t* events,
    int i);

/*
 * Purpose : Return a JSON representation of the event in the format expected
 *           by New Relic's backend.
 */
extern const char* nr_analytics_event_json(const nr_analytics_event_t* event);

#endif /* NR_ANALYTICS_EVENTS_HDR */
