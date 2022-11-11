/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_WRAPREC_HASHMAP_HDR
#define PHP_WRAPREC_HASHMAP_HDR

#if LOOKUP_METHOD == LOOKUP_USE_WRAPREC_HASHMAP

typedef struct _nr_wraprec_hashmap_t nr_php_wraprec_hashmap_t;

// clang-format off
extern nr_php_wraprec_hashmap_t* nr_php_wraprec_hashmap_create(void);
extern void nr_php_wraprec_hashmap_destroy(nr_php_wraprec_hashmap_t** hashmap_ptr);
// clang-format on

#endif

#endif