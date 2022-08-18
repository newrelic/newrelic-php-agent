/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stdlib.h>

#include "nr_analytics_events.h"
#include "nr_analytics_events_private.h"
#include "util_memory.h"

/*
 * The transaction analytics event is represented as a JSON string
 * for simplicity.  This is done using a structure (rather than
 * 'typedef char analytics_event_t') for the structure type safety.
 *
 * This JSON format matches the format expected by New Relic's backend.
 */
struct _nr_analytics_event_t {
  char json[1];
};

/*
 * Convenience function provided for testing.
 */
nr_analytics_event_t* nr_analytics_event_create_from_string(const char* str) {
  return (nr_analytics_event_t*)nr_strdup(str);
}

static nr_analytics_event_t* nr_analytics_event_duplicate(
    const nr_analytics_event_t* event) {
  if (0 == event) {
    return 0;
  }
  return (nr_analytics_event_t*)nr_strdup(event->json);
}

const char* nr_analytics_event_json(const nr_analytics_event_t* event) {
  if (0 == event) {
    return 0;
  }
  return event->json;
}

nr_analytics_event_t* nr_analytics_event_create(
    const nrobj_t* builtin_fields,
    const nrobj_t* agent_attributes,
    const nrobj_t* user_attributes) {
  nr_analytics_event_t* event;
  nrobj_t* arr;
  char* json;
  nrobj_t* empty_hash;

  if (builtin_fields && (NR_OBJECT_HASH != nro_type(builtin_fields))) {
    return 0;
  }
  if (agent_attributes && (NR_OBJECT_HASH != nro_type(agent_attributes))) {
    return 0;
  }
  if (user_attributes && (NR_OBJECT_HASH != nro_type(user_attributes))) {
    return 0;
  }

  /*
   * When represented in JSON format, an event looks like this:
   *
   *  [
   *    { BUILTIN_FIELDS_HERE },
   *    { USER_ATTRIBUTES_HERE },
   *    { AGENT_ATTRIBUTES_HERE }
   *  ]
   */
  empty_hash = nro_new_hash();
  arr = nro_new_array();
  nro_set_array(arr, 1, builtin_fields ? builtin_fields : empty_hash);
  nro_set_array(arr, 2, user_attributes ? user_attributes : empty_hash);
  nro_set_array(arr, 3, agent_attributes ? agent_attributes : empty_hash);
  nro_delete(empty_hash);

  json = nro_to_json(arr);
  nro_delete(arr);
  event = nr_analytics_event_create_from_string(json);
  nr_free(json);

  return event;
}

void nr_analytics_event_destroy(nr_analytics_event_t** event_ptr) {
  nr_realfree((void**)event_ptr);
}

struct _nr_analytics_events_t {
  int events_allocated; /* Maximum number of events to store in this data
                           structure. */
  int events_used;      /* Number of events within this data structure */
  int events_seen;      /* Number of times "add event" was called */
  nr_analytics_event_t** events; /* Array of events */
};

int nr_analytics_events_max_events(const nr_analytics_events_t* events) {
  if (NULL == events) {
    return 0;
  }
  return events->events_allocated;
}

int nr_analytics_events_number_seen(const nr_analytics_events_t* events) {
  if (0 == events) {
    return 0;
  }
  return events->events_seen;
}

int nr_analytics_events_number_saved(const nr_analytics_events_t* events) {
  if (0 == events) {
    return 0;
  }
  return events->events_used;
}

#define NR_ANALYTICS_EVENTS_MAX_EVENTS_SANITY_CHECK (10 * 1000 * 1000)

nr_analytics_events_t* nr_analytics_events_create(int max_events) {
  if (max_events <= 0) {
    return NULL;
  }

  return nr_analytics_events_create_ex(max_events);
}

nr_analytics_events_t* nr_analytics_events_create_ex(int max_events) {
  nr_analytics_events_t* events;

  if (max_events > NR_ANALYTICS_EVENTS_MAX_EVENTS_SANITY_CHECK) {
    return 0;
  }

  events = (nr_analytics_events_t*)nr_zalloc(sizeof(nr_analytics_events_t));
  events->events = (nr_analytics_event_t**)nr_zalloc(
      max_events * sizeof(nr_analytics_event_t*));
  events->events_allocated = max_events;
  events->events_used = 0;
  events->events_seen = 0;

  return events;
}

void nr_analytics_events_destroy(nr_analytics_events_t** events_ptr) {
  int i;
  nr_analytics_events_t* events;

  if (0 == events_ptr) {
    return;
  }
  events = *events_ptr;
  if (0 == events) {
    return;
  }

  for (i = 0; i < events->events_used; i++) {
    nr_analytics_event_destroy(&events->events[i]);
  }

  nr_free(events->events);
  nr_realfree((void**)events_ptr);
}

void nr_analytics_events_add_event(nr_analytics_events_t* events,
                                   const nr_analytics_event_t* event,
                                   nr_random_t* rnd) {
  int replace_idx;

  if (0 == events) {
    return;
  }
  if (0 == event) {
    return;
  }

  events->events_seen++;

  if (!nr_analytics_events_is_sampling(events)) {
    events->events[events->events_used] = nr_analytics_event_duplicate(event);
    events->events_used++;
  } else {
    /*
     * If the reservoir is full, we sample using the following sampling
     * algorithm: http://xlinux.nist.gov/dads/HTML/reservoirSampling.html
     */
    replace_idx = nr_random_range(rnd, events->events_seen);

    if ((replace_idx >= 0) && (replace_idx < events->events_allocated)) {
      nr_analytics_event_destroy(&events->events[replace_idx]);
      events->events[replace_idx] = nr_analytics_event_duplicate(event);
    }
  }
}

const char* nr_analytics_events_get_event_json(nr_analytics_events_t* events,
                                               int i) {
  if (NULL == events) {
    return NULL;
  }
  if (i < 0) {
    return NULL;
  }
  if (i >= events->events_used) {
    return NULL;
  }

  return nr_analytics_event_json(events->events[i]);
}

bool nr_analytics_events_is_sampling(nr_analytics_events_t* events) {
  if (events->events_used < events->events_allocated) {
    return false;
  }
  return true;
}