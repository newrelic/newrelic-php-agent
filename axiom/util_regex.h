/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions related to compiling and using
 * Perl Compatible Regular Expressions (PCRE)
 */
#ifndef UTIL_REGEX_HDR
#define UTIL_REGEX_HDR

#include "nr_axiom.h"
#include "util_buffer.h"

/*
 * The opaque structure representing a compiled regular expression.
 */
typedef struct _nr_regex_t nr_regex_t;

/*
 * The opaque structure that contains the substrings found when a string is
 * matched against a regular expression.
 */
typedef struct _nr_regex_substrings_t nr_regex_substrings_t;

/*
 * Options that can be supplied to nr_regex_create. These map 1:1 to similarly
 * named PCRE options.
 */
#define NR_REGEX_ANCHORED (1 << 0)
#define NR_REGEX_CASELESS (1 << 1)
#define NR_REGEX_DOLLAR_ENDONLY (1 << 2)
#define NR_REGEX_DOTALL (1 << 3)
#define NR_REGEX_MULTILINE (1 << 4)

/*
 * Purpose : Compile a regular expression.
 *
 * Params  : 1. A PCRE pattern.
 *           2. Any options that should be applied to the regular expression.
 *           3. Non-zero if extra time should be spent studying the regular
 *              expression. In general, this should only be enabled for regular
 *              expressions that are going to be used more than once.
 *
 * Returns : A regular expression.
 */
extern nr_regex_t* nr_regex_create(const char* pattern,
                                   int options,
                                   int do_study);

/*
 * Pupose : Destroy a regular expression.
 *
 * Params : 1. A pointer to the regular expression.
 */
extern void nr_regex_destroy(nr_regex_t** regex_ptr);

/*
 * Purpose : Match a string against a regular expression.
 *
 * Params  : 1. The regular expression.
 *           2. The string to match.
 *           3. The length of the string.
 *
 * Returns : NR_SUCCESS if the string matched, or NR_FAILURE if the string did
 *           not match or an error occurred.
 */
extern nr_status_t nr_regex_match(const nr_regex_t* regex,
                                  const char* str,
                                  int str_len);

/*
 * Purpose : Match a string against a regular expression and capture the
 *           matched substring and any subpatterns.
 *
 * Params  : 1. The regular expression.
 *           2. The string to match.
 *           3. The length of the string.
 *
 * Returns : A substrings object, which the caller is responsible for
 *           destroying, or NULL if the string did not match the regular
 *           expression or if an error occurred.
 *
 *           Note that the substrings object MUST NOT outlive the regex object.
 */
extern nr_regex_substrings_t* nr_regex_match_capture(const nr_regex_t* regex,
                                                     const char* str,
                                                     int str_len);

/*
 * Purpose : Destroy a substrings object.
 *
 * Params  : 1. A pointer to the substrings object.
 */
extern void nr_regex_substrings_destroy(nr_regex_substrings_t** ss_ptr);

/*
 * Purpose : Return the number of matched subpatterns.
 *
 * Params  : 1. The substrings object.
 *
 * Returns : The number of matched subpatterns, 0 if the string as a whole
 *           matched but no subpatterns exist in the regular expression, or -1
 *           if an error occurred.
 */
extern int nr_regex_substrings_count(const nr_regex_substrings_t* ss);

/*
 * Purpose : Retrieve a subpattern or the matched string.
 *
 * Params  : 1. The substrings object.
 *           2. The 1-indexed index of the subpattern to return, or 0 to return
 *              the entire matched string.
 *
 * Returns : A copy of the subpattern or matched string, which the caller is
 *           responsible for freeing, or NULL if an error occurred or the index
 *           is out of bounds.
 */
extern char* nr_regex_substrings_get(const nr_regex_substrings_t* ss,
                                     int index);

/*
 * Purpose : Retrieve a named subpattern.
 *
 * Params  : 1. The substrings object.
 *           2. The name of the subpattern.
 *
 * Returns : A copy of the subpattern, which the caller is responsible for
 *           freeing, or NULL if an error occurred, the named subpattern does
 *           not exist, or the named subpattern did not capture due to being on
 *           an inactive branch of the regular expression.
 */
extern char* nr_regex_substrings_get_named(const nr_regex_substrings_t* ss,
                                           const char* name);

/*
 * Purpose : Retrieve the start and end offsets of the given subpattern or the
 *           whole match.
 *
 * Params  : 1. The substrings object.
 *           2. The 1-indexed index of the subpattern to return, or 0 to return
 *              the entire matched string.
 *           3. The destination array, which will have the offsets written into
 *              it.
 *
 * Returns : NR_SUCCESS or NR_FAILURE. If NR_FAILURE is returned, then the
 *           values in the destination array will be untouched.
 */
extern nr_status_t nr_regex_substrings_get_offsets(
    const nr_regex_substrings_t* ss,
    int index,
    int offsets[2]);

/*
 * Purpose : Return a pointer to a statically allocated string holding the PCRE
 *           library version.
 */
extern const char* nr_regex_pcre_version(void);

/*
 * Purpose : Quote the given string so that it can be used directly in a
 *           regular expression.
 *
 * Params  : 1. The string to quote.
 *           2. The length of the string.
 *           3. If set, a pointer to an integer that will be set to the length
 *              of the quoted string. That length does not include the NULL
 *              terminator.
 *
 * Returns : A new, NULL terminated string which is quoted. The caller is
 *           responsible for freeing this string.
 */
extern char* nr_regex_quote(const char* str,
                            size_t str_len,
                            size_t* quoted_len);

/*
 * Purpose : Quote the given string and add it to a buffer.
 *
 * Params  : 1. The buffer to add the quoted string to.
 *           2. The string to quote.
 *           3. The length of the string.
 */
extern void nr_regex_add_quoted_to_buffer(nrbuf_t* buf,
                                          const char* str,
                                          size_t str_len);

#endif /* UTIL_REGEX_HDR */
