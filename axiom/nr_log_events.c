/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stdlib.h>

#include "nr_log_events.h"
#include "util_memory.h"

struct _nr_log_events_t {
  int events_allocated;    /* Maximum number of events to store in this data
                              structure. */
  int events_used;         /* Number of events within this data structure */
  int events_seen;         /* Number of times "add event" was called */
  nr_log_event_t** events; /* Array of events */
};

#define NR_LOG_EVENTS_MAX_EVENTS_SANITY_CHECK (10 * 1000 * 1000)

nr_log_events_t* nr_log_events_create(int max_events) {
  nr_log_events_t* events;

  if (max_events <= 0) {
    return 0;
  }
  if (max_events > NR_LOG_EVENTS_MAX_EVENTS_SANITY_CHECK) {
    return 0;
  }

  events = (nr_log_events_t*)nr_zalloc(sizeof(nr_log_events_t));
  events->events
      = (nr_log_event_t**)nr_zalloc(max_events * sizeof(nr_log_event_t*));
  events->events_allocated = max_events;
  events->events_used = 0;
  events->events_seen = 0;

  return events;
}

void nr_log_events_destroy(nr_log_events_t** events_ptr) {
  int i;
  nr_log_events_t* events;

  if (0 == events_ptr) {
    return;
  }
  events = *events_ptr;
  if (0 == events) {
    return;
  }

  for (i = 0; i < events->events_used; i++) {
    nr_log_event_destroy(&events->events[i]);
  }

  nr_free(events->events);
  nr_realfree((void**)events_ptr);
}
void nr_log_events_add_event(nr_log_events_t* events,
                             nr_log_event_t* event,
                             nr_random_t* rnd) {
  int replace_idx;

  if (0 == events) {
    return;
  }
  if (0 == event) {
    return;
  }

  events->events_seen++;

  if (events->events_used < events->events_allocated) {
    events->events[events->events_used]
        = event;  // nr_log_event_duplicate(event);
    events->events_used++;
  } else {
    /*
     * If the reservoir is full, we sample using the following sampling
     * algorithm: http://xlinux.nist.gov/dads/HTML/reservoirSampling.html
     */
    replace_idx = nr_random_range(rnd, events->events_seen);

    if ((replace_idx >= 0) && (replace_idx < events->events_allocated)) {
      nr_log_event_destroy(&events->events[replace_idx]);
      events->events[replace_idx] = event;  // nr_log_event_duplicate(event);
    }
  }
}

int nr_log_events_number_seen(const nr_log_events_t* events) {
  if (0 == events) {
    return 0;
  }
  return events->events_seen;
}
int nr_log_events_number_saved(const nr_log_events_t* events) {
  if (0 == events) {
    return 0;
  }
  return events->events_used;
}

char* nr_log_events_get_event_json(nr_log_events_t* events, int i) {
  if (NULL == events) {
    return NULL;
  }
  if (i < 0) {
    return NULL;
  }
  if (i >= events->events_used) {
    return NULL;
  }
  return nr_log_event_to_json(events->events[i]);
}
