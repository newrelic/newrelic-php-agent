/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains API for the hashmap that's used to lookup
 * the instrumentation of user functions.
 */
#ifndef PHP_USER_INSTRUMENT_HASHMAP_HDR
#define PHP_USER_INSTRUMENT_HASHMAP_HDR

#include "php_includes.h"
#include "php_user_instrument.h"

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
/*
 * The opaque hashmap type.
 */
typedef struct _nr_php_wraprec_hashmap nr_php_wraprec_hashmap_t;

/*
 * hashmap type stats
 */
typedef struct _nr_php_wraprec_hashmap_stats {
  size_t elements;
  size_t buckets_used;
  size_t collisions_min;
  size_t collisions_max;
  size_t collisions_mean;
  size_t buckets_with_collisions;
} nr_php_wraprec_hashmap_stats_t;

/*
 * Type declaration for destructor functions.
 */
typedef void (*nr_php_wraprec_hashmap_dtor_fn_t)(nruserfn_t*);
/*
 * Purpose : Create a hashmap with a set number of buckets.
 *
 * Params  : 1. The number of buckets. If this is not a power of 2, this will
 *              be rounded up to the next power of 2. The maximum value is
 *              2^24; values above this will be capped to 2^24.
 *           2. The destructor function, or NULL if not required.
 *
 * Returns : A newly allocated hashmap.
 */
extern nr_php_wraprec_hashmap_t* nr_php_wraprec_hashmap_create_buckets(
    size_t,
    nr_php_wraprec_hashmap_dtor_fn_t);

/*
 * Purpose : Destroy a hashmap.
 *
 * Params  : 1. The address of the hashmap to destroy.
 *
 * Returns : hashmap stats.
 */
extern nr_php_wraprec_hashmap_stats_t nr_php_wraprec_hashmap_destroy(
    nr_php_wraprec_hashmap_t**);

/*
 * Purpose : Update the key in the wraprec using metadata from zend function,
 *           and store updated wraprec pointer in the hashmap. An existing
 *           element with the same key will be overwritten by this function.
 *
 * Params  : 1. The hashmap.
 *           2. The zend function to set instrumentation for.
 *           3. The wraprec to set the key and store in the hashmap.
 *
 * Caveat: If zend function's zend_string metadata (function_name or filename)
 *         does not have hash calculated, this function will calculate value
 *         for zend_string's h property.
 */
extern void nr_php_wraprec_hashmap_update(nr_php_wraprec_hashmap_t*,
                                          zend_function*,
                                          nruserfn_t*);

/*
 * Purpose : Get a wraprec pointer from a hashmap into an out parameter.
 *
 * Params  : 1. The hashmap.
 *           2. The zend function to search for.
 *           3. A pointer to a void pointer that will be set to the element, if
 *              it exists. If the element doesn't exist, this value will be
 *              unchanged.
 *
 * Returns : Non-zero if the value exists, zero otherwise.
 *
 * Caveat: If zend function's zend_string metadata (function_name or filename)
 *         does not have hash calculated, this function will calculate value
 *         for zend_string's h property.
 */
extern int nr_php_wraprec_hashmap_get_into(nr_php_wraprec_hashmap_t*,
                                           zend_function*,
                                           nruserfn_t**);
#endif

#endif