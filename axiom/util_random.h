/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_RANDOM_HDR
#define UTIL_RANDOM_HDR

#include <stdint.h>

typedef struct _nr_random_t nr_random_t;

/*
 * Purpose : Return a new uniform random number generator (URNG).
 */
extern nr_random_t* nr_random_create(void);

/*
 * Purpose : Releases any resources associated with a random number generator.
 */
extern void nr_random_destroy(nr_random_t** rnd_ptr);

/*
 * Purpose : Set the seed value for a random number generator.  This can be
 *           used for deterministic testing.
 *
 * Params  : 1. The URNG to seed.
 *           2. The seed value. Only the lowest 32-bits are used.
 */
extern void nr_random_seed(nr_random_t* rnd, uint64_t seed);

/*
 * Purpose : Combine nr_random_create and nr_random_seed.
 */
extern nr_random_t* nr_random_create_from_seed(uint64_t seed);

/*
 * Purpose : Seed a random number generator from the host system's clock.
 */
extern void nr_random_seed_from_time(nr_random_t* rnd);

/*
 * Purpose : Generate a uniformly distributed integer over the interval
 *           [0, max_exclusive - 1].  0 will be returned if rnd is NULL, or
 *           max_exclusive is invalid.  max_exclusive may not be less than 2
 *           or greater than NR_RANDOM_MAX_EXCLUSIVE_LIMIT.
 */
#define NR_RANDOM_MAX_EXCLUSIVE_LIMIT ((unsigned long)(1U << 31))
extern unsigned long nr_random_range(nr_random_t* rnd,
                                     unsigned long max_exclusive);

/*
 * Purpose : Generate a uniformly distributed real number over the
 *           interval [0.0, 1.0).
 *
 * Returns : A uniformly distributed real number over the interval [0.0, 1.0).
 */
extern double nr_random_real(nr_random_t* rnd);

#endif /* UTIL_RANDOM_HDR */
