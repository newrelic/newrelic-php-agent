/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains routines to parse configuration strings.
 */
#ifndef NR_CONFIGSTRINGS_HDR
#define NR_CONFIGSTRINGS_HDR

#include "util_time.h"

/*
 * Purpose : Convert a string representation of time into an nrtime_t.
 *           Note that nrtime_t represents microseconds since the unix epoch.
 *
 * Params  : 1. The string to parse.  It contains a decimal formatted integer
 * with an optional suffix. Without a suffix, the integer encodes msecs. With a
 * suffix, the suffix is interpreted as one of the time scale factors.
 *           Allowable suffixes are:
 *             w  weeks
 *             d  days
 *             h  hours
 *             m  minutes
 *             s  seconds
 *             ms milliseconds
 *             us microseconds
 */
extern nrtime_t nr_parse_time(const char* str);

/*
 * Purpose : Parse a boolean from a string value, following the unsymmetrical
 *           rules of the PHP engine.
 *
 * Params  : 1. The string to parse.
 *
 * Notes :

 * - The PHP Engine delivers the given value false (without quotes) as
 *     0-length string.
 * - The PHP Engine delivers the given value "false" (with quotes) as the string
 *     "false".
 * - The PHP Engine delivers the given value true (without quotes) as 1-length
 *     string "1".
 * - The PHP Engine delivers the given value "true" (with quotes) to us as the
 *     string "true".
 *
 * Returns  1 if the value encodes "true".
 * Returns  0 if the value encodes "false".
 * Returns -1 if the value can't be parsed.
 */
extern int nr_bool_from_str(const char* str);

#endif /* NR_CONFIGSTRINGS_HDR */
