/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/resource.h>

#include "util_cpu.h"
#include "util_syscalls.h"

#define timeval_to_nrtime_t(T)                          \
  (nrtime_t)((((nrtime_t)(T).tv_sec) * NR_TIME_DIVISOR) \
             + (((nrtime_t)(T).tv_usec) * NR_TIME_DIVISOR_US))

nr_status_t nr_get_cpu_usage(nrtime_t* user_ptr, nrtime_t* sys_ptr) {
  int rv;
  struct rusage rusage;
  nrtime_t user;
  nrtime_t sys;

  if (user_ptr) {
    *user_ptr = 0;
  }
  if (sys_ptr) {
    *sys_ptr = 0;
  }

  rv = nr_getrusage(RUSAGE_SELF, &rusage);
  if (-1 == rv) {
    return NR_FAILURE;
  }

  user = timeval_to_nrtime_t(rusage.ru_utime);
  sys = timeval_to_nrtime_t(rusage.ru_stime);

  if (user_ptr) {
    *user_ptr = user;
  }
  if (sys_ptr) {
    *sys_ptr = sys;
  }

  return NR_SUCCESS;
}
