/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_GUID_HDR
#define NR_GUID_HDR

#include "util_random.h"

/*
 * The guid is an identifier that is unique to the transaction and is used
 * to tie this transaction trace to a browser trace and/or external
 * (cross application) traces.
 */
#define NR_GUID_SIZE 16

/*
 * Purpose : Create a new GUID.
 *
 * Params  : 1. The random number generator to use.
 *
 * Returns : A newly allocated, null terminated string, which is owned by the
 *           caller.
 */
extern char* nr_guid_create(nr_random_t* rnd);

#endif /* NR_GUID_HDR */
