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

#undef pcre
#undef pcre_extra
#undef pcre_compile
#undef pcre_study
#undef pcre_free
#undef pcre_copy_substring
#undef pcre_exec
#include <pcre.h>

#include "util_regex.h"

/*
 * The structure wrapping compiled regular expressions.
 */
struct _nr_regex_t {
  pcre* code;        /* compiled regular expression */
  pcre_extra* extra; /* extra study data, if requested, or NULL */
  int capture_count; /* the number of subpatterns that may be captured */
};

/*
 * The structure wrapping found substrings from a regular expression match.
 */
struct _nr_regex_substrings_t {
  pcre* code;        /* a copy of the compiled regular expression */
  char* subject;     /* a copy of the subject that was matched */
  int capture_count; /* the number of subpatterns that were captured */
  int* ovector;      /* the ovector array returned by PCRE */
  int ovector_size;  /* the number of elements in ovector */
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
 * Params  : 1. The PCRE code object used in the regular expression.
 *           2. The number of subpatterns matched. This may be 0 if there were
 *              no subpatterns.
 *
 * Returns : A new substrings object.
 */
extern nr_regex_substrings_t* nr_regex_substrings_create(pcre* code, int count);

#endif /* UTIL_REGEX_PRIVATE_HDR */
