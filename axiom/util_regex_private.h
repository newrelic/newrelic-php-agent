/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions related to compiling and using Perl-compatible
 * regular expressions.
 */
#ifndef UTIL_REGEX_PRIVATE_HDR
#define UTIL_REGEX_PRIVATE_HDR

/*
 * PCRE2 is built for a specific code unit width; we only deal with 8 bit
 * strings.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "util_regex.h"

/*
 * The structure wrapping compiled regular expressions.
 */
struct _nr_regex_t {
  pcre2_code* code;  /* compiled regular expression */
  int capture_count; /* the number of subpatterns that may be captured */
};

/*
 * The structure wrapping found substrings from a regular expression match.
 */
struct _nr_regex_substrings_t {
  pcre2_code* code;  /* the compiled regular expression (not owned) */
  char* subject;     /* a copy of the subject that was matched */
  int capture_count; /* the number of subpatterns that were captured */
  pcre2_match_data* match_data; /* the match data block; owns ovector */
  PCRE2_SIZE* ovector; /* the ovector array returned by PCRE2 */
  int ovector_size;    /* the number of elements in ovector */
};

/*
 * Purpose : Calculate the maximum number of subpatterns that may be matched by
 *           a regular expression.
 *
 * Params  : 1. The regular expression.
 *
 * Returns : The maximum number of subpatterns, or -1 if an error occurred.
 */
extern int nr_regex_capture_count(const nr_regex_t* regex);

/*
 * Purpose : Create a substrings object.
 *
 * Params  : 1. The PCRE2 code object used in the regular expression.
 *           2. The number of subpatterns matched. This may be 0 if there were
 *              no subpatterns.
 *
 * Returns : A new substrings object.
 */
extern nr_regex_substrings_t* nr_regex_substrings_create(pcre2_code* code,
                                                         int count);

#endif /* UTIL_REGEX_PRIVATE_HDR */
