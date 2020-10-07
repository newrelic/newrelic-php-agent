/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to support the sampling algorithm.
 */
#ifndef UTIL_SAMPLING_HDR
#define UTIL_SAMPLING_HDR

#include "nr_axiom.h"

#include <stdbool.h>

#include "util_random.h"

/*
 * The New Relic agent specification for the priority sampling algorithm offers
 * that all transactions and events must have an initial priority.  This
 * priority, when first generated, must be a random number between 0.0 and 1.0,
 * or [0.0, 1.0). These two constants represent the lower bound and upper bound.
 */
#define NR_PRIORITY_ERROR (-1.0)
#define NR_PRIORITY_LOWEST 0.0
#define NR_PRIORITY_HIGHEST 1.0

typedef double nr_sampling_priority_t;

/*
 * Purpose : Generate an initial priority.
 *
 * Params  : The random number generator to use.
 *
 * Returns : A newly generated priority or -1.0 if an error occurred.
 *
 */
static inline nr_sampling_priority_t nr_generate_initial_priority(
    nr_random_t* rnd) {
  return nr_random_real(rnd);
}

/*
 * Purpose : Compare two valid priority values.
 *
 * Params  : 1. The first priority to compare.
 *           2. The second priority to compare.
 *
 * Returns : true if the first priority is of higher priority than the
 *           second priority, false otherwise.
 */
static inline bool nr_is_higher_priority(nr_sampling_priority_t p1,
                                         nr_sampling_priority_t p2) {
  return p1 > p2;
}

/*
 * Purpose : Affirm that an initial priority value is valid.
 *
 * Params  : The priority to validate.
 *
 * Returns : true if the initial priority has a valid value, false otherwise.
 *
 */
static inline bool nr_priority_is_valid(nr_sampling_priority_t p) {
  return p != NR_PRIORITY_ERROR && p >= NR_PRIORITY_LOWEST
         && p < NR_PRIORITY_HIGHEST;
}
#endif /* UTIL_SAMPLING_HDR */
