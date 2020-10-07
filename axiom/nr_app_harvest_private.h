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
 * Params  : 1. A pointer to the app harvest.
 *           2. The current time.
 *
 * Returns : The time of the next harvest.
 */
extern nrtime_t nr_app_harvest_calculate_next_harvest_time(
    const nr_app_harvest_t* ah,
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
 * Params  : 1. The application harvest
 *           2. The current time.
 *
 * Returns : true if the application is in its first sampling period;
 *           false otherwise.
 */
extern bool nr_app_harvest_is_first(nr_app_harvest_t* ah, nrtime_t now);

/* The following functions shadow the public API in nr_app_harvest.h: the key
 * difference is that the current time is provided as an explicit parameter,
 * rather than coming from nr_get_time(). This is for testing purposes. */

extern void nr_app_harvest_private_init(nr_app_harvest_t* ah,
                                        nrtime_t connect_timestamp,
                                        nrtime_t harvest_frequency,
                                        uint16_t sampling_target,
                                        nrtime_t now);

extern bool nr_app_harvest_private_should_sample(nr_app_harvest_t* ah,
                                                 nr_random_t* rnd,
                                                 nrtime_t now);

#endif /* NR_APP_HARVEST_PRIVATE_HDR */
