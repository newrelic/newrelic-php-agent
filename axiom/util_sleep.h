/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains pause execution for various units of time.
 */
#ifndef UTIL_SLEEP_HDR
#define UTIL_SLEEP_HDR

#include "util_time.h"

/*
 * All these functions will return 0 if they slept for the full amount of time
 * exactly, a positive value if they were interrupted before their time could
 * elapse, or a negative value if they over-slept.
 */

/*
 * Purpose : Sleep for the number of milli-seconds.
 *
 * Params  : 1. The number of time unit to sleep for.
 *
 * Returns : The number of units left to sleep if the call was interrupted.
 */
extern int nr_msleep(int millis);

/*
 * Purpose : Parse a string representing Unix Epoch time.
 *
 * Params  : The time in a base-10 NUL-terminated string which may include a
 *           point ('.') as the decimal radix point.
 *           This conversion routine attempts to determine the
 *           appropriate scale factor for reasonable time span of early 21st
 *           century: The time may be in seconds, milliseconds, or nanoseconds.
 *
 * Returns : The time in nrtime_t units, or 0 upon failure.
 */
extern nrtime_t nr_parse_unix_time(const char* str);

#endif /* UTIL_SLEEP_HDR */
