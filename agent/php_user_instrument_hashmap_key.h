/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains structure and API for the key of the hashmap that's used
 * to lookup the instrumentation of user functions.
 */
#ifndef PHP_USER_INSTRUMENT_HASHMAP_KEY_HDR
#define PHP_USER_INSTRUMENT_HASHMAP_KEY_HDR

#include "php_includes.h"

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
/*
 * The hashmap key constructed from zend_function metadata
 */

typedef struct _nr_php_wraprec_hashmap_key {
  /* using refcounted zend_string for performance */
  zend_string* scope_name;
  zend_string* function_name;
  zend_string* filename;
  uint32_t lineno;
} nr_php_wraprec_hashmap_key_t;

/*
 * Purpose : Populate key with zend_function's metadata.
 *
 * Params  : 1. Pointer to the key to initialize
 *           2. Pointer to zend_function with metadata
 *
 * Returns : void
 */
extern void nr_php_wraprec_hashmap_key_set(nr_php_wraprec_hashmap_key_t*,
                                           const zend_function*);

/*
 * Purpose : Release zend_strings referenced by the key.
 *
 * Params  : 1. Pointer to the key with metadata to release
 *
 * Returns : void
 */
extern void nr_php_wraprec_hashmap_key_release(nr_php_wraprec_hashmap_key_t*);
#endif

#endif