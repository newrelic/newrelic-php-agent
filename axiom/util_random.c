/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "util_memory.h"
#include "util_random.h"
#include "util_time.h"

/*
 * Common state for uniform random number generators based on
 * the POSIX *rand48() family of linear congruential generators.
 */
struct _nr_random_t {
  unsigned short xsubi[3];
};

nr_random_t* nr_random_create(void) {
  nr_random_t* rnd;

  rnd = (nr_random_t*)nr_malloc(sizeof(*rnd));
  rnd->xsubi[0] = 0;
  rnd->xsubi[1] = 0;
  rnd->xsubi[2] = 0;

  return rnd;
}

void nr_random_destroy(nr_random_t** rnd_ptr) {
  nr_realfree((void**)rnd_ptr);
}

void nr_random_seed(nr_random_t* rnd, uint64_t seed) {
  if (0 == rnd) {
    return;
  }

  /*
   * Mimic POSIX srand48().
   *  1. Set the upper 32 bits of xsubi to the lower 32 bits of seed.
   *  2. Set the lower 16 bits of xsubi to 0x330e.
   */
  rnd->xsubi[2] = (seed & 0xffffffffl) >> 16;
  rnd->xsubi[1] = seed & 0xffffl;
  rnd->xsubi[0] = 0x330e;
}

nr_random_t* nr_random_create_from_seed(uint64_t seed) {
  nr_random_t* rnd = nr_random_create();

  nr_random_seed(rnd, seed);

  return rnd;
}

void nr_random_seed_from_time(nr_random_t* rnd) {
  nr_random_seed(rnd, nr_get_time());
}

unsigned long nr_random_range(nr_random_t* rnd, unsigned long max_exclusive) {
  unsigned long largest_multiple;

  if (0 == rnd) {
    return 0;
  }

  if (max_exclusive <= 1) {
    return 0;
  }

  if (max_exclusive > NR_RANDOM_MAX_EXCLUSIVE_LIMIT) {
    return 0;
  }

  /*
   * Unforunately, 'nrand48 () % max_exclusive' would not generate
   * random numbers evenly in the range [0, max_exclusive - 1], since
   * NR_RANDOM_MAX_EXCLUSIVE_LIMIT % max_exclusive may not be equal to zero.
   * Therefore we generate random numbers until we get a value that is less
   * than (the largest multiple of max_exclusive that is less than
   * NR_RANDOM_MAX_EXCLUSIVE_LIMIT) before performing the modulo.
   *
   * Please see:
   * http://www.azillionmonkeys.com/qed/random.html
   * http://stackoverflow.com/questions/10690839/how-to-get-pseudo-random-uniformly-distributed-integers-in-c-good-enough-for-sta
   * http://pubs.opengroup.org/onlinepubs/9699919799/functions/drand48.html
   */
  largest_multiple = NR_RANDOM_MAX_EXCLUSIVE_LIMIT
                     - (NR_RANDOM_MAX_EXCLUSIVE_LIMIT % max_exclusive);

  for (;;) {
    unsigned long x = (unsigned long)nrand48(rnd->xsubi);

    if (x < largest_multiple) {
      return x % max_exclusive;
    }
  }

  /* NOTREACHED */
}

double nr_random_real(nr_random_t* rnd) {
  if (0 == rnd) {
    return -1.0;
  }

  return erand48(rnd->xsubi);
}
