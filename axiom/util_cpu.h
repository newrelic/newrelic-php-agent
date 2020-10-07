/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a function to get cpu usage information.
 */
#ifndef UTIL_CPU_HDR
#define UTIL_CPU_HDR

#include "nr_axiom.h"
#include "util_time.h"

/*
 * Purpose : Get the amount of time spent executing the current process in
 *           units of nrtime_t.
 *
 * Params  : 1. Pointer to location to store user mode execution time.
 *           2. Pointer to location to store system execution time.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_get_cpu_usage(nrtime_t* user_ptr, nrtime_t* sys_ptr);

#endif /* UTIL_CPU_HDR */
