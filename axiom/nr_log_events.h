/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_EVENTS_HDR
#define NR_LOG_EVENTS_HDR

#include "nr_log_event.h"
#include "util_random.h"
#include "util_vector.h"

typedef struct _nr_log_events_t nr_log_events_t;

/*
 * Purpose : Add a log event to a log event pool.
 *
 * Params  : 1. Log event pool
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

/*
 * Purpose : Create a log event pool of specified size.
 *
 * Params  : 1. Log event pool
 *
 *  * Returns : Allocated log event pool or NULL on failure.
 *
 * Notes   : A pool of size 0 is valid.
 */
extern nr_log_events_t* nr_log_events_create(size_t max_events);

/*
 * Purpose : Destrory a log event pool.
 *
 * Params  : 1. Log event pool
 *
 */
extern void nr_log_events_destroy(nr_log_events_t** events);

/*
 * Purpose : Get the maximum number of events held by a event pool.
 *
 * Params  : 1. Log event pool
 *
 * Returns : Maximum number of events held by an event pool.
 *
 */
extern size_t nr_log_events_max_events(const nr_log_events_t* events);

/*
 * Purpose : Get the number of log events seen by a event pool.
 *
 * Params  : 1. Log event pool
 *
 * Returns : Number of log events seen by a event pool.
 *
 */
extern size_t nr_log_events_number_seen(const nr_log_events_t* events);

/*
 * Purpose : Get the number of log events saved by a event pool.
 *
 * Params  : 1. Log event pool
 *
 * Returns : Number of log events saved by a event pool.
 *
 */
extern size_t nr_log_events_number_saved(const nr_log_events_t* events);

/*
 * Purpose : See if log events are being sampled by log event pool.
 *
 * Params  : 1. Log event pool
 *
 * Returns : TRUE if log events are sampled (meaning some are dropped).
 *
 */
extern bool nr_log_events_is_sampling(nr_log_events_t* events);

/*
 * Purpose : Convert a log event pool to a vector containing the log events.
 *
 * Params  : 1. Log event pool
 *           2. Vector (must be allocated by caller)
 *
 * Notes   : The vector contains pointers into the log event pool so
 *           the log event pool must not be changed or destroyed
 *           while the vector is in use.
 */
extern void nr_log_events_to_vector(nr_log_events_t* events,
                                    nr_vector_t* vector);

int nr_log_event_wrapped_priority_comparator(const void* a,
                                             const void* b,
                                             void* userdata NRUNUSED);

#endif /* NR_LOG_EVENTS_HDR */