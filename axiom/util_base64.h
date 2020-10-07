/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to encode and decode Base64 data.
 */
#ifndef UTIL_BASE64_HDR
#define UTIL_BASE64_HDR

/*
 * Purpose : Encode an input string into Base64
 *
 * Params  : 1. The original data to encode
 *           2. The length of the data
 *           3. Pointer to an int to hold the length of the result
 *
 * Returns : The allocated encoded data or NULL on error, and the length of
 *           the encoded data in *retlen, if it wasn't NULL.
 */
extern char* nr_b64_encode(const char* data, int len, int* retlen);

/*
 * Purpose : Decode a Base64 string returning the original data
 *
 * Params  : 1. The Base64 string.
 *           2. Pointer to an int to hold the returned data length.
 *
 * Returns : The allocated data or NULL on error, and the length of the decoded
 *           data in *retlen, if it wasn't NULL.
 */
extern char* nr_b64_decode(const char* src, int* retlen);

/*
 * Purpose : Returns the table of characters used to encode/decode.
 *           For test integration purposes only.
 *
 * Returns : Returns the table of characters used to encode/decode.
 */
extern const char* nr_b64_get_table(void);

static inline int nr_b64_is_valid_character(char c) {
  if (('=' == c) || ('/' == c) || ('+' == c) || (('0' <= c) && (c <= '9'))
      || (('a' <= c) && (c <= 'z')) || (('A' <= c) && (c <= 'Z'))) {
    return 1;
  }
  return 0;
}

#endif /* UTIL_BASE64_HDR */
