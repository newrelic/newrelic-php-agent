/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with transaction segment terms
 * returned by the collector.
 */
#ifndef NR_SEGMENT_TERMS_HDR
#define NR_SEGMENT_TERMS_HDR

#include "util_object.h"

/*
 * Forward declaration of our opaque segment terms "object" type.
 */
typedef struct _nr_segment_terms_t nr_segment_terms_t;

/*
 * Purpose : Creates a new segment terms object. These contain sets of rules
 *           that should be applied to transaction names.
 *
 * Params  : 1. The maximum number of rules that the object can contain.
 *
 * Returns : A new segment terms object.
 */
extern nr_segment_terms_t* nr_segment_terms_create(int size);

/*
 * Purpose : Creates a new segment terms object from the JSON returned by the
 *           collector.
 *
 * Params  : 1. The JSON.
 *
 * Returns : A new segment terms object.
 */
extern nr_segment_terms_t* nr_segment_terms_create_from_obj(const nrobj_t* obj);

/*
 * Purpose : Destroys a segment terms object.
 *
 * Params  : 1. A pointer to the object, which will be set to NULL once
 *              destroyed.
 */
extern void nr_segment_terms_destroy(nr_segment_terms_t** terms_ptr);

/*
 * Purpose : Adds a rule to the given segment terms object.
 *
 * Params  : 1. The segment terms object.
 *           2. The rule prefix.
 *           3. An array containing the whitelist of terms to apply.
 *
 * Returns : NR_SUCCESS if the rule was valid and added, NR_FAILURE otherwise.
 */
extern nr_status_t nr_segment_terms_add(nr_segment_terms_t* segment_terms,
                                        const char* prefix,
                                        const nrobj_t* terms);

/*
 * Purpose : Adds a rule to the segment terms object based on a JSON rule in
 *           the correct format.
 *
 * Params  : 1. The segment terms object.
 *           2. A JSON object containing "prefix" and "terms" keys.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_segment_terms_add_from_obj(
    nr_segment_terms_t* segment_terms,
    const nrobj_t* rule);

/*
 * Purpose : Applies the transaction segment terms ruleset to the given
 *           transaction name.
 *
 * Params  : 1. The segment terms object.
 *           2. The transaction name.
 *
 * Returns : A newly allocated string containing the transformed transaction
 *           name, or NULL if the parameters are invalid.
 */
extern char* nr_segment_terms_apply(const nr_segment_terms_t* segment_terms,
                                    const char* name);

#endif
