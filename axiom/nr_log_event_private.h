/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_LOG_EVENT_PRIVATE_H
#define NR_LOG_EVENT_PRIVATE_H

#include "nr_attributes.h"
struct _nr_log_event_t {
  char* message;
  char* log_level;
  nrtime_t timestamp;
  nr_attributes_t* context_attributes;
  char* trace_id;
  char* span_id;
  char* entity_guid;
  char* entity_name;
  char* hostname;
  int priority;
};

#endif /* NR_LOG_EVENT_PRIVATE_H */
