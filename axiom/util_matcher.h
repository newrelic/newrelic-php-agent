/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_MATCHER_HDR
#define UTIL_MATCHER_HDR

#include <stdbool.h>

typedef struct _nr_matcher_t nr_matcher_t;

extern nr_matcher_t* nr_matcher_create(void);

extern void nr_matcher_destroy(nr_matcher_t** matcher_ptr);

extern bool nr_matcher_add_prefix(nr_matcher_t* matcher, const char* prefix);

extern char* nr_matcher_match(nr_matcher_t* matcher, const char* input);

extern char* nr_matcher_match_core(nr_matcher_t* matcher, const char* input);

#endif
