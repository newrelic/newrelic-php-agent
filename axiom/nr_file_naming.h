/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file is used for file transaction naming.
 */
#ifndef NR_FILE_NAMING_HDR
#define NR_FILE_NAMING_HDR

#include "util_regex.h"

struct _nr_file_naming_t;
typedef struct _nr_file_naming_t nr_file_naming_t;

/*
 * Purpose : Destroy a nr_file_naming_t linked list.
 *
 * Params  : 1. The list to destroy.
 */
extern void nr_file_namer_destroy(nr_file_naming_t** namer_ptr);

/*
 * Purpose : Check a filename against the defined patterns for transaction file
 * naming.
 *
 * Params  : 1. The patterns to match against.
 *           2. The filename to match.
 *
 * Returns : The matched string or NULL. See note on what exactly is returned.
 *
 * Note    : In the case of paths ending with a slash ('path/'), this matches
 *           'path/\.*' -- that is, path/., path/.., path/..., etc. Matching
 *           path/.. seems like strange behavior, but it's historical, so it's
 *           still here.
 */
extern char* nr_file_namer_match(const nr_file_naming_t* namer,
                                 const char* filename);

/*
 * Purpose : Appends a new file namer to the head of the given list.
 *
 * Params  : 1. The head of a list (or NULL to make a new one)
 *           2. A regular expression (generally from newrelic.ini). See
 *              nr_file_namer_match for information on matching.
 *
 * Returns : the list.
 */
extern nr_file_naming_t* nr_file_namer_append(nr_file_naming_t* curr_head,
                                              const char* user_pattern);

#endif /* NR_FILE_NAMING_HDR */
