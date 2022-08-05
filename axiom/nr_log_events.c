/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <math.h>
#include <stdlib.h>

#include "nr_log_events.h"
#include "nr_analytics_events_private.h"
#include "util_memory.h"

bool nr_log_events_add_event(nr_analytics_events_t* events,
                             const nr_log_event_t* event,
                             nr_random_t* rnd) {
  bool events_sampled = false;
  char* log_event_json = NULL;
  nr_analytics_event_t* log_event = NULL;

<<<<<<< HEAD
  if (NULL == events) {
    return false;
  }
  if (NULL == event) {
=======
  if (0 == events) {
    return false;
  }
  if (0 == event) {
>>>>>>> 25f7831 (feat(axiom): create `Logging/Forwarding/Dropped` metric)
    return false;
  }

  events_sampled = nr_analytics_events_is_sampling(events);

  log_event_json = nr_log_event_to_json(event);
  log_event = nr_analytics_event_create_from_string(log_event_json);
  nr_free(log_event_json);

  nr_analytics_events_add_event(events, log_event, rnd);
  nr_free(log_event);

  return events_sampled;
}