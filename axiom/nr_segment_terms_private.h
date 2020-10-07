/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with transaction segment terms.
 */
#ifndef NR_SEGMENT_TERMS_PRIVATE_HDR
#define NR_SEGMENT_TERMS_PRIVATE_HDR

#include "util_object.h"
#include "util_regex.h"

/*
 * A transaction segment terms rule.
 */
typedef struct _nr_segment_terms_rule_t {
  char* prefix;   /* The prefix to match before applying term rules. */
  int prefix_len; /* The length of the prefix. */
  nr_regex_t* re; /* The regexp that matches invalid terms. */
} nr_segment_terms_rule_t;

/*
 * A set of transaction segment terms rules.
 */
struct _nr_segment_terms_t {
  int capacity;                    /* The maximum number of rules. */
  int size;                        /* The actual number of rules. */
  nr_segment_terms_rule_t** rules; /* The array of rules. */
};

/*
 * Purpose : Creates a new rule.
 *
 * Params  : 1. The rule prefix.
 *           2. The array of terms to whitelist.
 *
 * Returns : A new rule object.
 */
extern nr_segment_terms_rule_t* nr_segment_terms_rule_create(
    const char* prefix,
    const nrobj_t* terms);

/*
 * Purpose : Destroys a rule.
 *
 * Params  : 1. The rule to destroy.
 */
extern void nr_segment_terms_rule_destroy(nr_segment_terms_rule_t** rule_ptr);

/*
 * Purpose : Applies the rule.
 *
 * Params  : 1. The rule to apply.
 *           2. The transaction name.
 *
 * Returns : A newly allocated string if the rule matched and was applied, or
 *           NULL if the rule was not matched.
 */
extern char* nr_segment_terms_rule_apply(const nr_segment_terms_rule_t* rule,
                                         const char* name);

/*
 * Purpose : Builds a regex that matches the term whitelist.
 *
 * Params  : 1. The array of terms to whitelist.
 *
 * Returns : A newly allocated string containing the regex, or NULL if an error
 *           occurred.
 */
extern char* nr_segment_terms_rule_build_regex(const nrobj_t* terms);

#endif
