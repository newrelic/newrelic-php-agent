/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions dealing with New Relic command replies.
 * It is essentially a thin layer over the objects utilities in util_object.c.
 */
#ifndef UTIL_REPLY_HDR
#define UTIL_REPLY_HDR

#include "util_object.h"

/*
 * Support functions for extracting real values out of a New Relic command
 * reply. These do error checking and return sensible values in the face of an
 * error.
 */

/*
 * Purpose : Extract an int, int64_t, boolean or double, with defaults, from
 *           the reply returned by a call to the New Relic platform.
 *
 * Params  : 1. The reply returned by nr_rpm_execute() above.
 *           2. The hash key for the element you wish to extract. If this is
 *              NULL, then it is assumed that parameter 1 is some other JSON
 *              child pointer and not a hash, and is simply extracting the
 *              value from that JSON child.
 *           3. The default value to return if there is an error or the element
 *              is not present in the New Relic command reply.
 *
 * Returns : The value of the hash key, or the default value if it was not
 *           present.
 *
 * Notes   : nr_reply_get_bool() does not simply check for 0 and 1 values, it
 *           will also check for strings like "true", "false", "on", "off",
 *           "yes", and "no".
 */
extern int nr_reply_get_int(const nrobj_t* reply, const char* name, int dflt);
extern int nr_reply_get_bool(const nrobj_t* reply, const char* name, int dflt);
extern double nr_reply_get_double(const nrobj_t* reply,
                                  const char* name,
                                  double dflt);

#endif /* UTIL_REPLY_HDR */
