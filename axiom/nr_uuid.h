/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for simple uuid generation.
 */
#ifndef NR_UUID_HDR
#define NR_UUID_HDR

#define NR_UUID_SIZE 32
#define NR_UUID_RANGE 16

/**
 * nr_uuid_create: pseudo-implementation of uuid generation logic
 *
 * This function will simply return a randomly generated 32 character hex
 * string. It does not implement the spec for UUID generation, which requires
 * specific adherence to implementation details and setting bits within the UUID
 * to signify which UUID generation variant was used.
 *
 * Params: int seed (<1 for time based seed)
 *
 * Returns: 32 byte hex string (must be freed by caller)
 */
char* nr_uuid_create(int seed);
#endif
