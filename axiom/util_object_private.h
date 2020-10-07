/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_OBJECT_PRIVATE_HDR
#define UTIL_OBJECT_PRIVATE_HDR

#include "util_object.h"

/*
 * Purpose : Assert that the given object is of the specified type.
 *
 * Params  : 1. The object.
 *           2. An object type.
 *
 * Returns : The object if successful, or NULL if not.
 *
 * Note    : Use nro_cassert for constant objects.
 */
extern nrobj_t* nro_assert(nrobj_t* obj, nrotype_t type);

#endif /* UTIL_OBJECT_PRIVATE_HDR */
