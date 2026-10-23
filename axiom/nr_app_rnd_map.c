/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_app.h"
#include "util_hashmap.h"
#include "util_random.h"

void nr_app_rnd_dtor(nr_random_t* rnd) {
  nr_random_destroy(&rnd);
}

nr_random_t* nr_app_get_or_create_thread_rnd(nrapp_t* app, uint64_t key) {
  nr_random_t* rnd;

  if (NULL == app || NULL == app->rnd_map) {
    return NULL;
  }

  rnd = (nr_random_t*)nr_hashmap_index_get(app->rnd_map, key);
  if (rnd) {
    return rnd;
  }

  rnd = nr_random_create();
  nr_random_seed_from_time(rnd);

  if (NR_SUCCESS != nr_hashmap_index_set(app->rnd_map, key, rnd)) {
    nr_random_destroy(&rnd);
    return NULL;
  }

  return rnd;
}
