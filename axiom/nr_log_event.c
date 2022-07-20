/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_memory.h"
#include "nr_attributes.h"
#include "util_strings.h"
#include "nr_log_event.h"
#include "nr_log_event_private.h"

nr_log_event_t* nr_log_event_create() {
  nr_log_event_t* le;

  le = (nr_log_event_t*)nr_malloc(sizeof(nr_log_event_t));

  le->trace_id = NULL;
  le->message = NULL;
  le->log_level = NULL;
  le->timestamp = 0;
  le->trace_id = NULL;
  le->span_id = NULL;
  le->entity_guid = NULL;
  le->entity_name = NULL;
  le->hostname = NULL;

  return le;
}

void nr_log_event_destroy(nr_log_event_t** ptr) {
  nr_log_event_t* event = NULL;

  if ((NULL == ptr) || (NULL == *ptr)) {
    return;
  }

  event = *ptr;
  nr_free(event->trace_id);
  nr_free(event->log_level);
  nr_free(event->message);
  nr_free(event->trace_id);
  nr_free(event->span_id);
  nr_free(event->entity_guid);
  nr_free(event->entity_name);
  nr_free(event->hostname);

  nr_realfree((void**)ptr);
}

char* nr_log_event_to_json(const nr_log_event_t* event) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  char* json = NULL;

  if (nr_log_event_to_json_buffer(event, buf)) {
    nr_buffer_add(buf, NR_PSTR("\0"));
    json = nr_strdup(nr_buffer_cptr(buf));
  }

  nr_buffer_destroy(&buf);
  return json;
}

bool nr_log_event_to_json_buffer(const nr_log_event_t* event, nrbuf_t* buf) {
  if (NULL == event || NULL == buf) {
    return false;
  }

  // We'll build the JSON manually to avoid copying the hashes into a new
  // nrobj_t array, which is expensive and pointless given it's a fixed length
  // array.
  nr_buffer_add(buf, NR_PSTR("["));
  nr_buffer_add(buf, NR_PSTR("{"));
  nr_buffer_add(buf, NR_PSTR("\"message\":\""));
  if (NULL == event->message) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->message, nr_strlen(event->message));
  }

  nr_buffer_add(buf, NR_PSTR("\",\"log.level\":\""));
  if (NULL == event->log_level) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->log_level, nr_strlen(event->log_level));
  }

  nr_buffer_add(buf, NR_PSTR("\",\"timestamp\":"));
  nr_buffer_write_uint64_t_as_text(buf, event->timestamp);

  nr_buffer_add(buf, NR_PSTR(",\"trace.id\":\""));
  if (NULL == event->trace_id) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->trace_id, nr_strlen(event->trace_id));
  }

  nr_buffer_add(buf, NR_PSTR("\",\"span.id\":\""));
  if (NULL == event->span_id) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->span_id, nr_strlen(event->span_id));
  }

  nr_buffer_add(buf, NR_PSTR("\",\"entity.guid\":\""));
  if (NULL == event->entity_guid) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->entity_guid, nr_strlen(event->entity_guid));
  }

  nr_buffer_add(buf, NR_PSTR("\",\"entity.name\":\""));
  if (NULL == event->entity_name) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->entity_name, nr_strlen(event->entity_name));
  }

  nr_buffer_add(buf, NR_PSTR("\",\"hostname\":\""));
  if (NULL == event->hostname) {
    nr_buffer_add(buf, NR_PSTR("null"));
  } else {
    nr_buffer_add(buf, event->hostname, nr_strlen(event->hostname));
  }
  nr_buffer_add(buf, NR_PSTR("\"}"));
  nr_buffer_add(buf, NR_PSTR("]"));

  return true;
}

void nr_log_event_set_message(nr_log_event_t* event, const char* message) {
  if (NULL == event || NULL == message) {
    return;
  }
  event->message = nr_strdup( message);
}

void nr_log_event_set_log_level(nr_log_event_t* event, const char* log_level) {
  if (NULL == event || NULL == log_level) {
    return;
  }
  event->log_level = nr_strdup( log_level);
}

void nr_log_event_set_timestamp(nr_log_event_t* event, const nrtime_t timestamp) {
  if (NULL == event) {
    return;
  }
  event->timestamp = timestamp;
}

void nr_log_event_set_trace_id(nr_log_event_t* event, const char* trace_id) {
  if (NULL == event || NULL == trace_id) {
    return;
  }
  event->trace_id = nr_strdup( trace_id);
}

void nr_log_event_set_span_id(nr_log_event_t* event, const char* span_id) {
  if (NULL == event || NULL == span_id) {
    return;
  }
  event->span_id = nr_strdup( span_id);
}

void nr_log_event_set_guid(nr_log_event_t* event, const char* guid) {
  if (NULL == event || NULL == guid) {
    return;
  }
  event->entity_guid = nr_strdup( guid);
}

void nr_log_event_set_entity_name(nr_log_event_t* event, const char* entity_name) {
  if (NULL == event || NULL == entity_name) {
    return;
  }
  event->entity_name = nr_strdup(entity_name);
}

void nr_log_event_set_hostname(nr_log_event_t* event, const char* hostname) {
  if (NULL == event || NULL == hostname) {
    return;
  }
  event->hostname = nr_strdup( hostname);
}
