/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to generate hashes from arbitrary strings.
 */
#ifndef UTIL_HASH_HDR
#define UTIL_HASH_HDR

#include <stdint.h>

#include "nr_axiom.h"

/*
 * Purpose : Compute a new path hash for outgoing CAT requests.
 *
 * Params  : 1. The name of the current transaction.
 *           2. The application name.
 *           3. The referring path hash, or NULL if there is no referring hash.
 *
 * Returns : A newly allocated string containing the new hash, or NULL if an
 *           error occurred.
 *
 */
extern char* nr_hash_cat_path(const char* txn_name,
                              const char* primary_app_name,
                              const char* referring_path_hash);

/*
 * Purpose : Compute the MD5 hash for the given string.
 *
 * Params  : 1. The 16 byte array to place the hash into.
 *           2. The string to compute the hash for.
 *           3. The length of the string, in bytes.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_hash_md5(unsigned char result[16],
                               const char* input,
                               int size);

/*
 * Purpose : Compute a non-cryptographic hash of a string.
 *
 * Params  : 1. The string to compute the function hash on. The empty string end
 *              NULL will always both evaluate to 0.
 *           2. An int pointer where the string length is stored. If this is a
 *              NULL pointer or currently points to a 0 value, the string MUST
 *              be NULL terminated. It's length will then be computed, and
 *              stored here.
 *
 * Returns : The hashed string.
 *
 * Note    : This function uses the MurmurHash3 algorithm. Up until and
 *           including PHP agent release 4.21 we used JSHash. MurmurHash3 is
 *           faster and experiences fewer collisions. The Calling code should
 *           not rely on a particular algorithm as this can and will change over
 *           time.
 */
extern uint32_t nr_mkhash(const char* str, int* len);

#endif /* UTIL_HASH_HDR */
