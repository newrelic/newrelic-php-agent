/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_app.h"
#include "util_hashmap.h"
#include "util_memory.h"

void nr_app_composer_status_dtor(nr_composer_api_status_t* status) {
  nr_free(status);
}

nr_composer_api_status_t* nr_app_get_or_create_thread_composer_status(
    nrapp_t* app,
    uint64_t key) {
  nr_composer_api_status_t* status;

  if (NULL == app || NULL == app->composer_map) {
    return NULL;
  }

  status
      = (nr_composer_api_status_t*)nr_hashmap_index_get(app->composer_map, key);
  if (status) {
    return status;
  }

  status
      = (nr_composer_api_status_t*)nr_malloc(sizeof(nr_composer_api_status_t));
  *status = NR_COMPOSER_API_STATUS_UNSET;

  if (NR_SUCCESS != nr_hashmap_index_set(app->composer_map, key, status)) {
    nr_free(status);
    return NULL;
  }

  return status;
}
