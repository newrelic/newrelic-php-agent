/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_EVENTS_HDR
#define NR_LOG_EVENTS_HDR

#include "nr_log_event.h"
#include "util_random.h"

/*
 * This is a pool of log events.
 */
typedef struct _nr_log_events_t nr_log_events_t;

/*
 * Purpose : Create a data structure to hold log events data.
 *
 * Params  : 1. The total number of events that will be recorded.  After this
 *              maximum is reached, events will be saved/replaced using
 *              a sampling algorithm.
 *
 * Returns : A newly allocated events structure, or 0 on error.
 */
extern nr_log_events_t* nr_log_events_create(int max_events);

/*
 * Purpose : Destroy an log event data structure, freeing all of its
 *           associated memory.
 */
extern void nr_log_events_destroy(nr_log_events_t** events_ptr);

/*
 * Purpose : Add an event to an event pool.
 *
 * Notes   : This function is not guaranteed to add an event: Once the events
 *           data structure is full, this event may replace an existing event
 *           based upon a sampling algorithm.
 */
extern void nr_log_events_add_event(nr_log_events_t* events,
                                    nr_log_event_t* event,
                                    nr_random_t* rnd);

/*
 * Purpose : Get the number of events that were attempted to be put in the
 *           structure using add_event.
 */
extern int nr_log_events_number_seen(const nr_log_events_t* events);

/*
 * Purpose : Get the number of events saved within the structure.
 */
extern int nr_log_events_number_saved(const nr_log_events_t* events);

/*
 * Purpose : Get event JSON from an event pool.
 */
extern char* nr_log_events_get_event_json(nr_log_events_t* events, int i);

#endif