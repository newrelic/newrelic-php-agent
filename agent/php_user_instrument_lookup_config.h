/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_LOOKUP_CONFIG
#define PHP_LOOKUP_CONFIG


/* See php_user_instrument_lookup.h for details of each method */
#define LOOKUP_USE_OP_ARRAY 0
#define LOOKUP_USE_LINKED_LIST 1
#define LOOKUP_USE_UTIL_HASHMAP 2
#define LOOKUP_USE_WRAPREC_HASHMAP 3

#ifndef LOOKUP_METHOD

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
/* Use new method for PHPs 7.4+ */
#define LOOKUP_METHOD LOOKUP_USE_WRAPREC_HASHMAP
#else
/* Use legacy method for PHPs older than 7.4 */
#define LOOKUP_METHOD LOOKUP_USE_OP_ARRAY
#endif

#endif

#endif