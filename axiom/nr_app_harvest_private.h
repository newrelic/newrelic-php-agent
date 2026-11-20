/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Private functions related to estimating application harvests for sampling
 * purposes.
 */
#ifndef NR_APP_HARVEST_PRIVATE_HDR
#define NR_APP_HARVEST_PRIVATE_HDR

/*
 * Purpose : Calculate the time of the next harvest for the given application.
 *
 * Params  : 1. A pointer to the harvest config.
 *           2. The current time.
 *
 * Returns : The time of the next harvest.
 */
extern nrtime_t nr_app_harvest_calculate_next_harvest_time(
    const nr_app_harvest_config_t* cfg,
    nrtime_t now);

/*
 * Purpose : Calculate the adaptive sampling threshold based on the target and
 *           the number of transactions seen in the current sampling period.
 *
 * Params  : 1. The target number of transactions that should be sampled.
 *           2. The number of transactions seen in the current sampling period.
 *
 * Returns : The threshold.
 */
extern uint64_t nr_app_harvest_calculate_threshold(uint64_t target,
                                                   uint64_t seen);

/*
 * Purpose : Determine if the current time is after the first sampling period.
 *
 * Params  : 1. The connect_timestamp of the application.
 *           2. The frequency of the application.
 *           3. The current time.
 *
 * Returns : true if the application's connect_timestamp + frequency
 *           are less than the current time; false otherwise.
 */
extern bool nr_app_harvest_compare_harvest_to_now(nrtime_t connect_timestamp,
                                                  nrtime_t frequency,
                                                  nrtime_t now);

/*
 * Purpose : Determine if the current time is after the first sampling period.
 *
 * Params  : 1. A pointer to the harvest config.
 *           2. The current time.
 *
 * Returns : true if the application is in its first sampling period;
 *           false otherwise.
 */
extern bool nr_app_harvest_is_first(const nr_app_harvest_config_t* cfg,
                                    nrtime_t now);

/* The following functions accept an explicit current time rather than calling
 * nr_get_time() internally.  This is for testing purposes. */

/*
 * Params  : 1. A pointer to the harvest config.
 *           2. A pointer to the harvest stats.
 *           3. A pointer to a random number generator.
 *           4. The current time.
 */
extern bool nr_app_harvest_private_should_sample(
    const nr_app_harvest_config_t* cfg,
    nr_app_harvest_stats_t* ah,
    nr_random_t* rnd,
    nrtime_t now);

#endif /* NR_APP_HARVEST_PRIVATE_HDR */
