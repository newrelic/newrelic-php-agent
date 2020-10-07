/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides time and duration functions.
 */
#ifndef UTIL_TIME_HDR
#define UTIL_TIME_HDR

#include <sys/time.h>

#include <inttypes.h>
#include <stdint.h>

typedef uint64_t nrtime_t; /* Microseconds since the UNIX epoch */

#define NR_TIME_FMT "%" PRIu64
#define NR_TIME_MAX ULONG_MAX
/*
 * We have two main uses of time in the agent. The first is to measure a
 * particular instant in time for reporting purposes. That is, we want to
 * know when NOW is. The second is to measure the elapsed time between two
 * events, that is the difference between END and START. The two requirements
 * have greatly divergent requirements in terms of resolution. For the first,
 * measuring NOW, simple 1 second accuracy is enough. For the second, the
 * difference between two events, the greater the accuracy the better. We have
 * the ability to measure this with nanosecond accuracy on some systems, and
 * in microseconds on most. We thus use the lowest common denominator of
 * microseconds. However, the New Relic backend still wants time in fractional
 * seconds as a double; doing so requires using doubles, so we delay that as
 * long as is humanly possible and only do the conversion when we are preparing
 * the JSON output for the New Relic backend. Up to that point in time, we deal
 * with microseconds.
 */
#define NR_TIME_DIVISOR \
  ((nrtime_t)1000000) /* How many units of time in a second */
#define NR_TIME_DIVISOR_SQUARE ((nrtime_t)1000000000000) /* Squared */
#define NR_TIME_DIVISOR_MS ((nrtime_t)1000) /* Convert to milliseconds */
#define NR_TIME_DIVISOR_US ((nrtime_t)1)    /* Convert to microseconds */

#define NR_TIME_DIVISOR_D ((double)1000000.0)
#define NR_TIME_DIVISOR_D_SQUARE ((double)1000000000000.0)
#define NR_TIME_DIVISOR_MS_D ((double)1000.0) /* Convert to milliseconds */
#define NR_TIME_DIVISOR_US_D ((double)1.0)    /* Convert to microseconds */

/*
 * This doesn't fit with the normal naming scheme (it should be nr_time_get),
 * but is maintained for backward compatibility for now.
 *
 * The return value is in microseconds
 */
static inline nrtime_t nr_get_time(void) {
  struct timeval tv;
  nrtime_t ret;

  (void)gettimeofday(&tv, 0);
  ret = tv.tv_sec * NR_TIME_DIVISOR;
  ret = ret + tv.tv_usec;
  return ret;
}

/*
 * Purpose: Calculate a time duration, the difference between a given
 *          start and stop time, each measured in microseconds since the
 *          UNIX Epoch.
 */
static inline nrtime_t nr_time_duration(nrtime_t start, nrtime_t stop) {
  if (start > stop) {
    return 0;
  }

  return stop - start;
}

#endif /* UTIL_TIME_HDR */
