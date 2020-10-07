/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_LOGGING_PRIVATE_HDR
#define UTIL_LOGGING_PRIVATE_HDR

#include <sys/time.h>

#include <stddef.h>

extern void nrl_format_timestamp(char* buf,
                                 size_t buflen,
                                 const struct timeval* tv);

#endif /* UTIL_LOGGING_HDR */
