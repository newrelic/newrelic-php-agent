/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with synthetics headers.
 */
#ifndef NR_SYNTHETICS_HDR
#define NR_SYNTHETICS_HDR

/*
 * Forward declaration of the opaque synthetics type.
 */
typedef struct _nr_synthetics_t nr_synthetics_t;

/*
 * Purpose : Creates a new synthetics object from a decoded
 *           X-NewRelic-Synthetics header.
 *
 * Params  : 1. The decoded value of the header.
 *
 * Returns : A new synthetics object, or NULL on error.
 */
extern nr_synthetics_t* nr_synthetics_create(const char* header);

/*
 * Purpose : Destroys a synthetics object.
 *
 * Params  : 1. The object to destroy.
 */
extern void nr_synthetics_destroy(nr_synthetics_t** synthetics_ptr);

/*
 * Purpose : Returns the version of the synthetics header.
 *
 * Params  : 1. The synthetics object.
 *
 * Returns : The version number.
 */
extern int nr_synthetics_version(const nr_synthetics_t* synthetics);

/*
 * Purpose : Returns the account ID in the synthetics header.
 *
 * Params  : 1. The synthetics object.
 *
 * Returns : The account ID.
 */
extern int nr_synthetics_account_id(const nr_synthetics_t* synthetics);

/*
 * Purpose : Returns the resource ID in the synthetics header.
 *
 * Params  : 1. The synthetics object.
 *
 * Returns : The resource ID.
 */
extern const char* nr_synthetics_resource_id(const nr_synthetics_t* synthetics);

/*
 * Purpose : Returns the job ID in the synthetics header.
 *
 * Params  : 1. The synthetics object.
 *
 * Returns : The job ID.
 */
extern const char* nr_synthetics_job_id(const nr_synthetics_t* synthetics);

/*
 * Purpose : Returns the monitor ID in the synthetics header.
 *
 * Params  : 1. The synthetics object.
 *
 * Returns : The monitor ID.
 */
extern const char* nr_synthetics_monitor_id(const nr_synthetics_t* synthetics);

/*
 * Purpose : Returns the value of the X-NewRelic-Synthetics header to add to
 *           all outbound requests. This will usually need to be encoded before
 *           transmission.
 *
 * Params  : 1. The synthetics object.
 *
 * Returns : The value of the header to add, or NULL if this isn't a synthetics
 *           request.
 */
extern const char* nr_synthetics_outbound_header(nr_synthetics_t* synthetics);

#endif /* NR_SYNTHETICS_HDR */
