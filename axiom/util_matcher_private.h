/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_MATCHER_PRIVATE_HDR
#define UTIL_MATCHER_PRIVATE_HDR

#include "util_vector.h"

typedef struct _nr_matcher_t {
  nr_vector_t prefixes;
} nr_matcher_t;

#endif /* UTIL_MATCHER_PRIVATE_HDR */
