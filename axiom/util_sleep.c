/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * All of these sleep functions are implemented in terms of nanosleep(),
 * which means that they do not affect process timers, and there is no special
 * handling for SIGALRM.
 */
#include "nr_axiom.h"

#include <errno.h>
#include <math.h>
#include <time.h>

#include "util_number_converter.h"
#include "util_sleep.h"
#include "util_time.h"

/*
 * These absolute times are used to determine the correct units of the input.
 */
#define NR_EARLIEST_ACCEPTABLE_UNIX_TIME \
  ((nrtime_t)(946684800ULL) * NR_TIME_DIVISOR) /* 2000/1/1 */
#define NR_LATEST_ACCEPTABLE_UNIX_TIME \
  ((nrtime_t)(2524629600ULL) * NR_TIME_DIVISOR) /* 2050/1/1 */

nrtime_t nr_parse_unix_time(const char* str) {
  double db;
  nrtime_t val;

  if ((0 == str) || (0 == str[0])) {
    return 0;
  }

  db = nr_strtod(str, 0);
  if ((0.0 == db) || (db < 0.0) || (HUGE_VAL == db)) {
    return 0;
  }

  /*
   * The value provided could be is usec, msec, or seconds. Since we know
   * it is supposed to be a Unix Time, we can check the reasonableness of
   * each of these possibilities.
   */
  val = (nrtime_t)(db * NR_TIME_DIVISOR_US_D);
  if ((val > NR_EARLIEST_ACCEPTABLE_UNIX_TIME)
      && (val < NR_LATEST_ACCEPTABLE_UNIX_TIME)) {
    return val;
  }

  val = (nrtime_t)(db * NR_TIME_DIVISOR_MS_D);
  if ((val > NR_EARLIEST_ACCEPTABLE_UNIX_TIME)
      && (val < NR_LATEST_ACCEPTABLE_UNIX_TIME)) {
    return val;
  }

  val = (nrtime_t)(db * NR_TIME_DIVISOR_D);
  if ((val > NR_EARLIEST_ACCEPTABLE_UNIX_TIME)
      && (val < NR_LATEST_ACCEPTABLE_UNIX_TIME)) {
    return val;
  }

  return 0;
}

int nr_msleep(int millis) {
  struct timespec ts;
  struct timespec remaining;
  int rv;

  ts.tv_sec = millis / 1000;
  millis = millis - (ts.tv_sec * 1000);
  ts.tv_nsec = millis * 1000 * 1000;

  rv = nanosleep(&ts, &remaining);
  if (EINTR == rv) {
    return (int)(remaining.tv_sec * 1000)
           + (int)(remaining.tv_nsec / (1000 * 1000));
  }
  return 0;
}
