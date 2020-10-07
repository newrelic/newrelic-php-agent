/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a function to return constant strings for errors.
 *
 * When reporting errors it is more meaningful in the log file to display the
 * error name, not the number, and also to have platform-independent strings
 * for the various errors. That is what these two functions do. They return
 * pointers to constant strings so they are inherently thread-safe.
 */
#ifndef UTIL_ERRNO_HDR
#define UTIL_ERRNO_HDR

/*
 * Purpose : Returns a human-readable string for a system error.
 *
 * Params  : 1. The system error number.
 *
 * Returns : The human-readable string for the error.
 */
extern const char* nr_errno(int errnum);

#endif /* UTIL_ERRNO_HDR */
