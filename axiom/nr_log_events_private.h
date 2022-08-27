/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains log events data internals.
 */
#ifndef NR_LOG_EVENTS_PRIVATE_HDR
#define NR_LOG_EVENTS_PRIVATE_HDR

//#include "nr_log_event.h"
#include "util_minmax_heap.h"

struct _nr_log_events_t {
  size_t events_allocated;  /* Maximum number of events to store in this data
                            structure. */
  size_t events_used;       /* Number of events within this data structure */
  size_t events_seen;       /* Number of times "add event" was called */
  nr_minmax_heap_t* events; /* heap for log event storeg */
};

#endif /* NR_LOG_EVENTS_PRIVATE_HDR */
