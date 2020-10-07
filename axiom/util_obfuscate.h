/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions dealing with string obfuscation.
 */
#ifndef UTIL_OBFUSCATE_H
#define UTIL_OBFUSCATE_H

/*
 * Purpose : Obfuscate and Base64 encode the given string.
 *
 * Params  : 1. The raw string.
 *           2. The key string.
 *           3. Key string length. Calculated if 0. This allows RUM
 *              obfuscations to use the first 13 characters of the browser
 *              monitoring key.
 *
 * Returns : A pointer to a newly allocated string on success, and 0 on failure.
 */
char* nr_obfuscate(const char* str, const char* key, int keylen);

/*
 * Purpose : Base64 decode and de-obfuscate the given string.
 *
 * Params  : 1. The encoded and obfuscated string.
 *           2. The key string.
 *           3. Key string length. Calculated if 0.
 *
 * Returns : A pointer to a newly allocated string on success, and 0 on failure.
 */
char* nr_deobfuscate(const char* str, const char* key, int keylen);

#endif
