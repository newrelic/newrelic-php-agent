/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A minimal set type, which uses only pointer equality to determine membership.
 */
#ifndef UTIL_SET_HDR
#define UTIL_SET_HDR

#include <stdbool.h>
#include <stddef.h>

/*
 * The opaque set type.
 */
typedef struct _nr_set_t nr_set_t;

/*
 * Purpose : Create a set.
 *
 * Returns : An empty set.
 */
extern nr_set_t* nr_set_create(void);

/*
 * Purpose : Destroy a set.
 *
 * Params  : A pointer to the set to be destroyed.
 */
extern void nr_set_destroy(nr_set_t** set_ptr);

/*
 * Purpose : Test if the given pointer is contained in the set.
 *
 * Params  : 1. The set.
 *           2. The pointer to check.
 *
 * Returns : True if the pointer is in the set; false otherwise.
 */
extern bool nr_set_contains(nr_set_t* set, const void* value);

/*
 * Purpose : Insert the given pointer into the set.
 *
 * Params  : 1. The set.
 *           2. The pointer to insert.
 */
extern void nr_set_insert(nr_set_t* set, const void* value);

/*
 * Purpose : Return the current size of the set.
 *
 * Params  : 1. The set.
 *
 * Returns : The number of unique pointers within the set.
 */
extern size_t nr_set_size(const nr_set_t* set);

#endif /* UTIL_SET_HDR */
