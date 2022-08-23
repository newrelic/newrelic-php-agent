/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stdlib.h>

#include "nr_limits.h"
#include "nr_log_events.h"
#include "nr_log_events_private.h"
#include "nr_log_event_private.h"
#include "util_memory.h"
#include "util_minmax_heap.h"
#include "util_vector.h"

bool nr_log_events_is_sampling(nr_log_events_t* events) {
  if (events->events_used < events->events_allocated) {
    return false;
  }
  return true;
}

/*
 * Safety check for comparator functions
 *
 * This avoids NULL checks in each comparator and ensures that NULL
 * elements are consistently considered as smaller.
 */
#define COMPARATOR_NULL_CHECK(__elem1, __elem2)             \
  if (nrunlikely(NULL == (__elem1) || NULL == (__elem2))) { \
    if ((__elem1) < (__elem2)) {                            \
      return -1;                                            \
    } else if ((__elem1) > (__elem2)) {                     \
      return 1;                                             \
    }                                                       \
    return 0;                                               \
  }

static int nr_log_event_age_comparator(const nr_log_event_t* a,
                                       const nr_log_event_t* b) {
  if (a->timestamp < b->timestamp) {
    return -1;
  } else if (a->timestamp > b->timestamp) {
    return 1;
  }
  return 0;
}

static int nr_log_event_wrapped_age_comparator(const void* a, const void* b) {
  COMPARATOR_NULL_CHECK(a, b);

  return nr_log_event_age_comparator((const nr_log_event_t*)a,
                                     (const nr_log_event_t*)b);
}

static int nr_log_event_priority_comparator(const nr_log_event_t* a,
                                            const nr_log_event_t* b) {
  if (a->priority > b->priority) {
    return 1;
  } else if (a->priority < b->priority) {
    return -1;
  } else {
    return nr_log_event_wrapped_age_comparator(a, b);
  }
}

int nr_log_event_wrapped_priority_comparator(const void* a,
                                             const void* b,
                                             void* userdata NRUNUSED) {
  COMPARATOR_NULL_CHECK(a, b);

  return nr_log_event_priority_comparator((const nr_log_event_t*)a,
                                          (const nr_log_event_t*)b);
}

/* All log events popping out of the segment heap go through this
 * function.
 *
 * The log event is discarded (thus removed from the log event heap).
 */
static void nr_log_event_discard_wrapper(nr_log_event_t* event,
                                         void* userdata NRUNUSED) {
  if (NULL == event) {
    return;
  }

  nr_log_event_destroy(&event);
}

nr_log_events_t* nr_log_events_create(int max_events) {
  nr_log_events_t* events;

  if (NR_MAX_LOG_EVENTS_MAX_SAMPLES_STORED < max_events) {
    return NULL;
  }

  events = (nr_log_events_t*)nr_zalloc(sizeof(nr_log_events_t));
  events->events_allocated = max_events;
  events->events_used = 0;
  events->events_seen = 0;

  if (0 != max_events) {
    events->events = nr_minmax_heap_create(
        max_events, nr_log_event_wrapped_priority_comparator, NULL,
        (nr_minmax_heap_dtor_t)nr_log_event_discard_wrapper, NULL);
  } else {
    events->events = NULL;
  }

  return events;
}

void nr_log_events_destroy(nr_log_events_t** events_ptr) {
  nr_log_events_t* events;

  if (NULL == events_ptr) {
    return;
  }
  events = *events_ptr;
  if (NULL == events) {
    return;
  }

  if (NULL != events->events) {
    nr_minmax_heap_destroy(&events->events);
  }

  nr_realfree((void**)events_ptr);
}

int nr_log_events_max_events(const nr_log_events_t* events) {
  if (NULL == events)
    return 0;

  return events->events_allocated;
}

int nr_log_events_number_seen(const nr_log_events_t* events) {
  if (NULL == events)
    return 0;

  return events->events_seen;
}

int nr_log_events_number_saved(const nr_log_events_t* events) {
  if (NULL == events)
    return 0;

  return events->events_used;
}

bool nr_log_events_add_event(nr_log_events_t* events,
                             const nr_log_event_t* event) {
  bool events_sampled = false;
  nr_log_event_t* dup_event;

  if (NULL == event) {
    return false;
  }

  // need to increment this if a valid event was sent
  // so take care if it here then sanity check other args
  if (NULL != events)
    events->events_seen++;

  // if no event queue exists or size is 0 then event will be dropped
  if (NULL == events || NULL == events->events
      || 0 == events->events_allocated) {
    return true;
  }

  events_sampled = nr_log_events_is_sampling(events);

  // make a copy of the event so the heap can control its entire life cycle
  dup_event = nr_log_event_clone(event);
  if (NULL == dup_event)
    return false;

  nr_minmax_heap_insert(events->events, (void*)dup_event);
  if (!events_sampled)
    events->events_used++;

  return events_sampled;
}

/*
 * Purpose : Place an nr_log_event_t pointer in a heap into a nr_set_t,
 *             or "heap to set".
 *
 * Params  : 1. The log event pointer in the heap.
 *           2. A void* pointer to be recast as the pointer to the set.
 *
 * Note    : This is the callback function supplied to nr_minmax_heap_iterate
 *           used for iterating over a heap of log events and placing each
 *           log events into a set.
 */
static bool nr_log_event_htov_iterator_callback(void* value, void* userdata) {
  if (nrlikely(value && userdata)) {
    nr_vector_t* vector = (nr_vector_t*)userdata;
    nr_vector_push_back(vector, value);
  }

  return true;
}

void nr_log_events_to_vector(nr_log_events_t* events, nr_vector_t* vector) {
  if (NULL == events || NULL == events->events || NULL == vector) {
    return;
  }

  /* Convert the heap to a set */
  nr_minmax_heap_iterate(
      events->events,
      (nr_minmax_heap_iter_t)nr_log_event_htov_iterator_callback,
      (void*)vector);

  return;
}
