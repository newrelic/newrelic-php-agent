/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_errors.h"
#include "nr_errors_private.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_time.h"

nr_error_t* nr_error_create(int priority,
                            const char* message,
                            const char* klass,
                            const char* stacktrace_json,
                            const char* span_id,
                            nrtime_t when) {
  nr_error_t* error;

  if (0 == message) {
    return 0;
  }
  if (0 == klass) {
    return 0;
  }
  if (0 == stacktrace_json) {
    return 0;
  }

  error = (nr_error_t*)nr_zalloc(sizeof(nr_error_t));
  error->priority = priority;
  error->when = when;
  error->message = nr_strdup(message);
  error->klass = nr_strdup(klass);
  error->stacktrace_json = nr_strdup(stacktrace_json);

  if (NULL != span_id) {
    error->span_id = nr_strdup(span_id);
  }
  return error;
}

const char* nr_error_get_message(const nr_error_t* error) {
  if (NULL == error) {
    return NULL;
  }
  return error->message;
}

const char* nr_error_get_klass(const nr_error_t* error) {
  if (NULL == error) {
    return NULL;
  }
  return error->klass;
}

nrtime_t nr_error_get_time(const nr_error_t* error) {
  if (NULL == error) {
    return 0;
  }
  return error->when;
}

int nr_error_priority(const nr_error_t* error) {
  if (0 == error) {
    return 0;
  }
  return error->priority;
}

const char* nr_error_get_span_id(const nr_error_t* error) {
  if (NULL == error) {
    return NULL;
  }
  return error->span_id;
}

void nr_error_destroy(nr_error_t** error_ptr) {
  nr_error_t* error;

  if (0 == error_ptr) {
    return;
  }
  error = *error_ptr;
  if (0 == error) {
    return;
  }
  nr_free(error->message);
  nr_free(error->klass);
  nr_free(error->span_id);
  nr_free(error->stacktrace_json);
  nr_realfree((void**)error_ptr);
}

static nrobj_t* nr_error_params_to_object(const char* stacktrace_json,
                                          const nrobj_t* agent_attributes,
                                          const nrobj_t* user_attributes,
                                          const nrobj_t* intrinsics,
                                          const char* request_uri) {
  nrobj_t* hash;
  char* json;

  hash = nro_new_hash();
  nro_set_hash_jstring(hash, "stack_trace", stacktrace_json);

  if (agent_attributes) {
    json = nro_to_json(agent_attributes);
    nro_set_hash_jstring(hash, "agentAttributes", json);
    nr_free(json);
  }

  if (user_attributes) {
    json = nro_to_json(user_attributes);
    nro_set_hash_jstring(hash, "userAttributes", json);
    nr_free(json);
  }

  if (intrinsics) {
    json = nro_to_json(intrinsics);
    nro_set_hash_jstring(hash, "intrinsics", json);
    nr_free(json);
  }

  if (request_uri) {
    nro_set_hash_string(hash, "request_uri", request_uri);
  }

  return hash;
}

char* nr_error_to_daemon_json(const nr_error_t* error,
                              const char* txn_name,
                              const nrobj_t* agent_attributes,
                              const nrobj_t* user_attributes,
                              const nrobj_t* intrinsics,
                              const char* request_uri) {
  nrobj_t* outer;
  nrobj_t* params;
  char* json;

  if (NULL == error) {
    return NULL;
  }

  /*
   * Since errors are not aggregated together in the daemon, we create the JSON
   * expected by the collector here, and send it to the daemon along with the
   * priority (so that the daemon can keep the highest priority errors).
   */

  params = nr_error_params_to_object(error->stacktrace_json, agent_attributes,
                                     user_attributes, intrinsics, request_uri);

  outer = nro_new_array();
  nro_set_array_long(outer, 1, error->when / NR_TIME_DIVISOR_MS);
  nro_set_array_string(outer, 2, txn_name);
  nro_set_array_string(outer, 3, error->message);
  nro_set_array_string(outer, 4, error->klass);
  nro_set_array(outer, 5, params);
  nro_delete(params);

  json = nro_to_json(outer);
  nro_delete(outer);

  return json;
}