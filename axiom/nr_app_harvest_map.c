/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_app.h"
#include "nr_app_harvest.h"
#include "util_hashmap.h"
#include "util_logging.h"
#include "util_memory.h"

void nr_app_harvest_stats_dtor(nr_app_harvest_stats_t* stats) {
  nr_free(stats);
}

static void nr_app_reset_thread_harvest_stats(void* value,
                                               const char* key NRUNUSED,
                                               size_t key_len NRUNUSED,
                                               void* user_data) {
  nr_app_harvest_stats_t* stats = (nr_app_harvest_stats_t*)value;
  nrapp_t* app = (nrapp_t*)user_data;

  if (NULL == stats || NULL == app) {
    return;
  }

  nr_app_harvest_stats_init(&app->adaptive_sampling_config, stats);
}

void nr_app_update_harvest_config(nrapp_t* app,
                                   nrtime_t connect_timestamp,
                                   nrtime_t frequency,
                                   uint16_t sampling_target) {
  nrtime_t prev_timestamp;
  nrtime_t prev_frequency;

  if (NULL == app) {
    return;
  }

  prev_timestamp = app->adaptive_sampling_config.connect_timestamp;
  prev_frequency = app->adaptive_sampling_config.frequency;

  app->adaptive_sampling_config.connect_timestamp = connect_timestamp;
  app->adaptive_sampling_config.frequency = frequency;
  app->adaptive_sampling_config.target_transactions_per_cycle = sampling_target;

  nrl_debug(NRL_AGENT,
            "Adaptive sampling configuration. Connect: " NR_TIME_FMT
            " us. Frequency: " NR_TIME_FMT " us. Target: %d.",
            connect_timestamp, frequency, sampling_target);

  if (connect_timestamp != prev_timestamp || frequency != prev_frequency) {
    nr_hashmap_apply(app->harvest_map, nr_app_reset_thread_harvest_stats, app);
  }
}

nr_app_harvest_stats_t* nr_app_get_or_create_thread_harvest(nrapp_t* app,
                                                             uint64_t key) {
  nr_app_harvest_stats_t* ah;

  if (NULL == app || NULL == app->harvest_map) {
    return NULL;
  }

  ah = (nr_app_harvest_stats_t*)nr_hashmap_index_get(app->harvest_map, key);
  if (ah) {
    return ah;
  }

  ah = (nr_app_harvest_stats_t*)nr_calloc(1, sizeof(nr_app_harvest_stats_t));
  nr_app_harvest_stats_init(&app->adaptive_sampling_config, ah);

  if (NR_SUCCESS != nr_hashmap_index_set(app->harvest_map, key, ah)) {
    nr_free(ah);
    return NULL;
  }

  return ah;
}
