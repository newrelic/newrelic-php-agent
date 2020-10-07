/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains structures and functions for URL rules, metrics rules,
 * and transaction name rules.
 */
#ifndef NR_RULES_HDR
#define NR_RULES_HDR

#include <stdint.h>

#include "util_object.h"

typedef struct _nrrules_t nrrules_t;

/*
 * Rule Flags
 */
#define NR_RULE_EACH_SEGMENT 0x00000001 /* Match each segment */
#define NR_RULE_IGNORE 0x00000002       /* Ignore this URL */
#define NR_RULE_REPLACE_ALL \
  0x00000004 /* Replace entire metric with substitution */
#define NR_RULE_TERMINATE \
  0x00000008 /* Terminate the rule chain if this rule applied */
#define NR_RULE_HAS_ALTS 0x00000010 /* Does this rule have | alternatives */
#define NR_RULE_HAS_CAPTURES                      \
  0x00000020 /* Does this rule have \0-9 captures \
              */

/*
 * If New Relic's backend does not specify an eval_order for a rule,
 * we'll use this one.
 */
#define NR_RULE_DEFAULT_ORDER 99999

/*
 * Purpose : Create a new rules table, with enough initial space for the given
 *           number of rules.
 *
 * Params  : 1. The number of rules to initially allocate space for.
 *
 * Returns : A pointer to the newly created table or NULL on error.
 *
 * Notes   : The argument provided can be used when the number of rules is
 *           known in advance. If rules beyond this number are added, the add
 *           function will extend the array 8 rules at a time.
 */
extern nrrules_t* nr_rules_create(int num);

/*
 * Purpose : Create a new rules table from a generic object.
 *
 * Returns : A pointer to the newly created rules or NULL on error.
 */
extern nrrules_t* nr_rules_create_from_obj(const nrobj_t* obj);

/*
 * Purpose : Destroy an existing rules table, releasing all resources.
 *
 * Params  : 1. A pointer to the return of nr_rules_create() above.
 *
 * Returns : Nothing.
 *
 * Notes   : Will set the variable that the argument points to to NULL after it
 *           has done its work.
 */
extern void nr_rules_destroy(nrrules_t** rules_p);

/*
 * Purpose : Add a new rule to a rule table.
 *
 * Params  : 1. The rule table to add the rule to.
 *           2. The flags for this rule (see NR_RULE_* above).
 *           3. The rule processing order.
 *           4. The string to match.
 *           5. The replacement string. Required unless the IGNORE flag is set.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Notes   : Flags like NR_RULE_HAS_ALTS and NR_RULE_HAS_CAPTURES are important
 *           for speed purposes when the rules are being processed.
 */
extern nr_status_t nr_rules_add(nrrules_t* rules,
                                uint32_t flags,
                                int order,
                                const char* match,
                                const char* repl);

/*
 * Purpose : Sort the rules table in rule processing order.
 *
 * Params  : 1. The rule table to sort.
 *
 * Returns : Nothing.
 *
 * Notes   : After adding 1 or more rules to a table, before it can be used the
 *           table must be sorted, according to the rule processing order field.
 *           If two rules have the same processing order field their exact
 *           ordering within the table is indeterminate and random. However,
 *           lower numbered rules will always appear before any higher numbered
 *           rule.
 */
extern void nr_rules_sort(nrrules_t* rules);

/*
 * Purpose : Apply rules to a string.
 *
 * Params  : 1. The rules to apply.
 *           2. The input string.
 *           3. Pointer to location to return changed string. Optional.
 *
 * Returns : One of the nr_rules_result_t values. If NR_RULES_RESULT_CHANGED
 *           is returned, then a malloc-ed string with the changed result will
 *           be returned through the 'new_name' parameter.
 */
typedef enum _nr_rules_result_t {
  NR_RULES_RESULT_IGNORE = 1,
  NR_RULES_RESULT_UNCHANGED = 2,
  NR_RULES_RESULT_CHANGED = 3
} nr_rules_result_t;

extern nr_rules_result_t nr_rules_apply(const nrrules_t* rules,
                                        const char* name,
                                        char** new_name);

#endif /* NR_RULES_HDR */
