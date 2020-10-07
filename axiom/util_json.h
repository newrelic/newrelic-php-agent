/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a function to format an escaped JSON string.
 */
#ifndef UTIL_JSON_HDR
#define UTIL_JSON_HDR

/*
 * Purpose : Produce a well-formed JSON string that is correctly escaped. The
 *           DEST must be large enough to accommodate the full string (so it
 *           should be at least six times the length of the input string).
 *           A NUL terminator is written to the end of the escaped JSON.
 *
 * Returns : The number of characters written to DEST, NOT including the NUL
 *           terminator, and 0 on error.
 *
 * Params  : 1. The destination buffer.
 *           2. The source buffer, null terminated.
 */
extern int nr_json_escape(char* dest, const char* json);

#endif /* UTIL_JSON_HDR */
