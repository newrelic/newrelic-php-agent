/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_RULES_PRIVATE_HDR
#define NR_RULES_PRIVATE_HDR

#include "nr_rules.h"
#include "util_object.h"
#include "util_regex.h"

#define NRULE_BUF_SIZE 2048

typedef struct _nrrule_t {
  int rflags;        /* Rule flags (see below) */
  int order;         /* Rule order */
  char* match;       /* Pattern to match */
  char* replacement; /* Replacement text */
  nr_regex_t* regex; /* Compiled RE */
} nrrule_t;

/*
 * A list of rules
 */
struct _nrrules_t {
  int nrules;      /* How many rules in the list */
  int nalloc;      /* Number of rules allocated */
  nrrule_t* rules; /* Actual list of rules */
};

extern void nr_rules_process_rule(nrrules_t* rules, const nrobj_t* rule);

extern void nr_rule_replace_string(const char* repl,
                                   char* dest,
                                   size_t dest_len,
                                   const nr_regex_substrings_t* ss);

#endif /* NR_RULES_PRIVATE_HDR */
