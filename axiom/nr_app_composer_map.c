/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_app.h"
#include "nr_php_packages.h"
#include "util_hashmap.h"
#include "util_memory.h"

void nr_app_composer_entry_dtor(nr_composer_thread_entry_t* entry) {
  if (NULL == entry) {
    return;
  }
  nr_php_packages_destroy(&entry->packages);
  nr_free(entry);
}

nr_composer_thread_entry_t* nr_app_get_or_create_thread_composer_entry(
    nrapp_t* app,
    uint64_t key) {
  nr_composer_thread_entry_t* entry;

  if (NULL == app || NULL == app->composer_map) {
    return NULL;
  }

  entry = (nr_composer_thread_entry_t*)nr_hashmap_index_get(app->composer_map,
                                                            key);
  if (entry) {
    return entry;
  }

  entry = (nr_composer_thread_entry_t*)nr_zalloc(
      sizeof(nr_composer_thread_entry_t));
  entry->status = NR_COMPOSER_API_STATUS_UNSET;

  if (NR_SUCCESS != nr_hashmap_index_set(app->composer_map, key, entry)) {
    nr_free(entry);
    return NULL;
  }

  return entry;
}
