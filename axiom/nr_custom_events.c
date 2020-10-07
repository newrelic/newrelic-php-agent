/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_attributes.h"
#include "nr_custom_events.h"
#include "util_logging.h"
#include "util_strings.h"

static nr_status_t nr_custom_events_iter(const char* key,
                                         const nrobj_t* val,
                                         void* ptr) {
  nr_attributes_t* atts = (nr_attributes_t*)ptr;
  uint32_t dest = NR_ATTRIBUTE_DESTINATION_TXN_EVENT;

  nr_attributes_user_add(atts, dest, key, val);

  return NR_SUCCESS;
}

const char nr_custom_event_valid_event_type_chars[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:_ ";

static int nr_custom_events_valid_event_type(const char* type) {
  int idx;

  if (NULL == type) {
    return 0;
  }
  if (nr_strlen(type) > NR_ATTRIBUTE_KEY_LENGTH_LIMIT) {
    nrl_warning(
        NRL_TXN,
        "unable to add custom event: type string exceeds length limit of %d",
        NR_ATTRIBUTE_KEY_LENGTH_LIMIT);
    return 0;
  }

  idx = nr_strspn(type, nr_custom_event_valid_event_type_chars);
  if ((idx <= 0) || (0 != type[idx])) {
    nrl_warning(NRL_TXN,
                "unable to add custom event: event type does not match "
                "^[a-zA-Z0-9:_ ]+$");
    return 0;
  }

  return 1;
}

void nr_custom_events_add_event(nr_analytics_events_t* custom_events,
                                const char* type,
                                const nrobj_t* params,
                                nrtime_t now,
                                nr_random_t* rnd) {
  nrobj_t* intrinsics;
  nr_analytics_event_t* event;
  nrobj_t* validated;
  nr_attributes_t* atts;

  if (NULL == params) {
    return;
  }
  if (0 == nr_custom_events_valid_event_type(type)) {
    return;
  }

  intrinsics = nro_new_hash();
  nro_set_hash_string(intrinsics, "type", type);
  nro_set_hash_double(intrinsics, "timestamp",
                      ((double)now) / NR_TIME_DIVISOR_D);

  /*
   * Custom events are not affected by attribute configuration.  However, we
   * use the attributes system here to validate/truncate the parameters.
   */
  atts = nr_attributes_create(NULL);
  nro_iteratehash(params, nr_custom_events_iter, atts);
  validated
      = nr_attributes_user_to_obj(atts, NR_ATTRIBUTE_DESTINATION_TXN_EVENT);

  event = nr_analytics_event_create(intrinsics, NULL, validated);
  nr_analytics_events_add_event(custom_events, event, rnd);

  nr_attributes_destroy(&atts);
  nr_analytics_event_destroy(&event);
  nro_delete(intrinsics);
  nro_delete(validated);
}
