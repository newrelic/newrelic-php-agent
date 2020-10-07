/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Structures and functions related to estimating application harvests for
 * sampling purposes.
 */
#ifndef NR_APP_HARVEST_HDR
#define NR_APP_HARVEST_HDR

#include "util_sampling.h"
#include "util_time.h"

typedef struct _nr_app_harvest_t {
  /* Fields we get from the daemon. */
  nrtime_t connect_timestamp; /* From the daemon: the timestamp the application
                               was connected */

  /* One must consider frequency and sampling_target together.  If frequency is
   * 60 and target_transactions_per_cycle is 10, this means the agent should aim
   * at sampling 10 samples per 60 seconds */
  nrtime_t frequency; /* From the daemon: sampling_target_period_in_seconds */
  uint64_t
      target_transactions_per_cycle; /* From the daemon: sampling_target.
                                      * The number of transactions we should
                                      * try to sample per cycle */

  /* Fields we calculate and update to estimate transaction volume and inform
   * sampling behaviour. */
  nrtime_t next_harvest; /* The timestamp of the next harvest, as best as the
                            agent can guess */
  uint64_t threshold;    /* Calculated by nr_app_harvest_calculate_threshold()
                            and updated here for unit-testing purposes. */
  uint64_t prev_transactions_seen; /* The number of transactions see in the last
                                      sampling period */
  uint64_t transactions_seen; /* The number of transactions seen in the current
                                 sampling period */
  uint64_t transactions_sampled; /* The number of transactions sampled in the
                                    current sampling period */
} nr_app_harvest_t;

/*
 * Purpose : Initialise the fields within the nr_app_harvest_t structure.
 *
 * Params  : 1. A pointer to the app harvest.
 *           2. The connect timestamp.
 *           3. The harvest frequency.
 *           4. The sampling target.
 */
extern void nr_app_harvest_init(nr_app_harvest_t* ah,
                                nrtime_t connect_timestamp,
                                nrtime_t harvest_frequency,
                                uint16_t sampling_target);

/*
 * Purpose : Check if the current transaction should be sampled.
 *
 * Params  : 1. A pointer to the app harvest.
 *           2. A pointer to a random number generator.
 *
 * Returns : True if the transaction should be sampled; false otherwise.
 *
 * Note    : This function has side effects: the transaction counters in the
 *           app harvest struct will be incremented assuming that each
 *           transaction will call this function once, and once only.
 */
extern bool nr_app_harvest_should_sample(nr_app_harvest_t* ah,
                                         nr_random_t* rnd);

#endif /* NR_APP_HARVEST_HDR */
