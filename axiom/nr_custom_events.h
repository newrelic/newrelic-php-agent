/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Custom events allow the user to add non-transaction events.
 */
#ifndef NR_CUSTOM_EVENTS_HDR
#define NR_CUSTOM_EVENTS_HDR

#include "nr_analytics_events.h"
#include "util_random.h"
#include "util_time.h"

/*
 * Purpose : Add a new custom event to an event pool.
 *
 * Params  : 1. The custom events being added to.
 *           2. A string which will be set as the "type" field in the event.
 *           3. A hash of key/value pairs.  The value of each should
 *              be a string, numeric, bool, or null.
 *           4. The current time.  This is passed as a parameter for easier
 *              unit testing.
 *           5. A random number generator to be used if sampling is required.
 */
extern void nr_custom_events_add_event(nr_analytics_events_t* custom_events,
                                       const char* type,
                                       const nrobj_t* params,
                                       nrtime_t now,
                                       nr_random_t* rnd);

#endif /* NR_CUSTOM_EVENTS_HDR */
