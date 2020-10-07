/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "nr_rules.h"
#include "nr_rules_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_reply.h"
#include "util_strings.h"

nrrules_t* nr_rules_create(int num) {
  nrrules_t* ret;

  if (num <= 0) {
    num = 8;
  }

  ret = (nrrules_t*)nr_zalloc(sizeof(nrrules_t));
  ret->nalloc = num;
  ret->rules = (nrrule_t*)nr_calloc(ret->nalloc, sizeof(nrrule_t));

  return (ret);
}

void nr_rules_destroy(nrrules_t** rules_p) {
  int i;
  nrrules_t* rules;

  if (nrunlikely((0 == rules_p) || (0 == *rules_p))) {
    return;
  }

  rules = *rules_p;

  for (i = 0; i < rules->nrules; i++) {
    nr_regex_destroy(&rules->rules[i].regex);
    nr_free(rules->rules[i].match);
    nr_free(rules->rules[i].replacement);
  }
  nr_free(rules->rules);
  rules->nrules = 0;
  rules->nalloc = 0;
  nr_realfree((void**)rules_p);

  return;
}

/*
 * Unfortunately, there is no spec for the rule regular expression options.
 * There is no consistency between agents regarding case sensivity.
 * Case-insensitivity is used here since it has been used historically.
 */
const int nr_rules_regex_options
    = NR_REGEX_CASELESS | NR_REGEX_DOLLAR_ENDONLY | NR_REGEX_DOTALL;

nr_status_t nr_rules_add(nrrules_t* rules,
                         uint32_t flags,
                         int order,
                         const char* match,
                         const char* repl) {
  nr_regex_t* regex;
  nrrule_t* rule;

  if (nrunlikely((0 == rules) || (0 == match) || (0 == match[0]))) {
    return NR_FAILURE;
  }

  regex = nr_regex_create(match, nr_rules_regex_options, 1);
  if (NULL == regex) {
    /*
     * nr_regex_create will also have logged the error message in this case.
     */
    nrl_warning(NRL_RULES, "RPM rule " NRP_FMT " failed to compile",
                NRP_REGEXP(match));
    return NR_FAILURE;
  }

  if (rules->nalloc == rules->nrules) {
    rules->nalloc += 8;
    rules->rules
        = (nrrule_t*)nr_realloc(rules->rules, rules->nalloc * sizeof(nrrule_t));
  }

  rule = &rules->rules[rules->nrules];
  rules->nrules++;
  nr_memset(rule, 0, sizeof(*rule));

  rule->rflags = flags;
  rule->order = order;
  rule->match = nr_strdup(match);
  if (repl && repl[0]) {
    rule->replacement = nr_strdup(repl);
  }
  rule->regex = regex;

  return NR_SUCCESS;
}

static int qsort_comparator_for_rules(const void* e1, const void* e2) {
  const nrrule_t* r1 = (const nrrule_t*)e1;
  const nrrule_t* r2 = (const nrrule_t*)e2;

  if (r1->order < r2->order) {
    return -1;
  }
  if (r1->order > r2->order) {
    return 1;
  }
  return 0;
}

void nr_rules_sort(nrrules_t* rules) {
  if ((0 == rules) || (0 == rules->rules) || (0 == rules->nrules)) {
    return;
  }

  qsort((void*)rules->rules, rules->nrules, sizeof(nrrule_t),
        qsort_comparator_for_rules);
}

void nr_rule_replace_string(const char* repl,
                            char* dest,
                            size_t dest_len,
                            const nr_regex_substrings_t* ss) {
  int state = 0;
  int ch;
  int num = 0;
  int len;
  int count = nr_regex_substrings_count(ss);

  if (0 == dest) {
    return;
  }

  if (0 == dest_len) {
    return;
  }

  while (0 != (ch = *repl)) {
    if (0 == state) {
      if ('\\' == ch) {
        state = 1;
        num = 0;
      } else if (dest_len > 0) {
        *dest = ch;
        dest++;
        dest_len--;
      }
    } else if (1 == state) {
      if ((ch >= '0') && (ch <= '9')) {
        num = num * 10;
        num += (ch - '0');
      }

      ch = repl[1];
      if ((ch >= '0') && (ch <= '9')) {
        /* EMPTY */;
      } else {
        state = 0;
        if (num > count) {
          *dest = 0;
          len = snprintf(dest, dest_len, "\\%d", num);
          dest += len;
          dest_len -= len;
        } else {
          char* sub = nr_regex_substrings_get(ss, num);

          if (sub) {
            len = snprintf(dest, dest_len, "%s", sub);
            dest += len;
            dest_len -= len;
          }

          nr_free(sub);
        }
      }
    }
    repl++;
  }
  *dest = 0;
}

/*
 * Purpose : Apply a rule to a string.
 *
 * Params  : 1. String which contains the input to be edited in place.
 *           2. Buffer space for intermediate work.
 *           3. Buffer space for replacement strings.
 *           4. The rule to apply.
 *
 * Returns : NR_RULES_RESULT_IGNORE, NR_RULES_RESULT_UNCHANGED, or
 *           NR_RULES_RESULT_CHANGED.
 */
static nr_rules_result_t nr_rule_apply(char* str,
                                       char* work,
                                       char* repl,
                                       size_t repl_len,
                                       const nrrule_t* rule) {
  int changed = 0;
  char* wp = work;

  if (nrunlikely((0 == str) || (0 == work) || (0 == repl) || (0 == rule))) {
    return NR_RULES_RESULT_UNCHANGED;
  }

  work[0] = 0;

  /*
   * The order in which we do the evaluation is important. We first do the
   * case where the ignore flag is set, as it is a simple match. If a match
   * is found, we return the ignore constant.
   *
   * Next we do the case where no special flags are set (that is, none of
   * ignore, each_segment or replace_all are set). That's because this
   * rule is a simple replace if any match is found rule, and all we need
   * to do is check for the match. If a match is found we make the replacement.
   *
   * The two more complicated cases are when we have either replace_all
   * or each_segment set. Of the two, each_segment is the most complicated
   * as we have to split the string into it's URL segments, and apply the
   * match and replacement on each segment. The replace_all case is a bit
   * easier because we do not have to split the original string, but we
   * do need to loop the match/replace, keeping track of where we were in
   * the original string as we go along.
   */
  if ((rule->rflags & NR_RULE_IGNORE)
      || (0 == (rule->rflags & (NR_RULE_EACH_SEGMENT | NR_RULE_REPLACE_ALL)))) {
    int slen = nr_strlen(str);
    nr_regex_substrings_t* ss;

    ss = nr_regex_match_capture(rule->regex, str, slen);
    if (ss) {
      int mstart;
      int mend;
      int offsets[2];

      if (NR_FAILURE == nr_regex_substrings_get_offsets(ss, 0, offsets)) {
        nr_regex_substrings_destroy(&ss);
        return NR_RULES_RESULT_UNCHANGED;
      }
      mstart = offsets[0];
      mend = offsets[1];

      changed++;

      if (rule->rflags & NR_RULE_IGNORE) {
        nr_regex_substrings_destroy(&ss);
        return NR_RULES_RESULT_IGNORE;
      }

      /* Copy the part before the match. */
      wp = nr_strxcpy(wp, str, mstart);

      /* Copy the match replacement. */
      if (rule->rflags & NR_RULE_HAS_CAPTURES) {
        nr_rule_replace_string(rule->replacement, repl, repl_len, ss);
        wp = nr_strlcpy(wp, repl, NRULE_BUF_SIZE);
      } else {
        wp = nr_strlcpy(wp, rule->replacement, NRULE_BUF_SIZE);
      }

      /* Copy the part after the match. */
      nr_strcpy(wp, str + mend);
      nr_strcpy(str, work);

      nr_regex_substrings_destroy(&ss);
    }
  } else if (rule->rflags & NR_RULE_EACH_SEGMENT) {
    char* segstart;
    char* segend;
    int tsl;

    segstart = str + 1; /* Skip first leading '/' */
    do {
      nr_regex_substrings_t* ss;

      segend = nr_strchr(segstart, '/');

      if (segend) {
        *segend = 0;
      }

      wp = nr_strcpy(wp, "/");
      tsl = nr_strlen(segstart);
      ss = nr_regex_match_capture(rule->regex, segstart, tsl);
      if (ss) {
        changed++;
        if (rule->rflags & NR_RULE_HAS_CAPTURES) {
          nr_rule_replace_string(rule->replacement, repl, repl_len, ss);
          wp = nr_strcpy(wp, repl);
        } else {
          wp = nr_strcpy(wp, rule->replacement);
        }

        nr_regex_substrings_destroy(&ss);
      } else {
        wp = nr_strcpy(wp, segstart);
      }
      if (segend) {
        segstart = segend + 1;
      }
    } while ((char*)0 != segend);
    /*
     * Note we need to copy the work buffer back to str regardless of whether or
     * not a change occurred since we have overriden the '/' characters of the
     * original string.
     */
    nr_strcpy(str, work);
  } else if (rule->rflags & NR_RULE_REPLACE_ALL) {
    /*
     * This is trickier than you'd think. PCRE allows you to match from any
     * position within the string, you do not have to start at the beginning.
     * Thus, the proper way to do the match is to resume the match from the
     * end of the last position that matched. The tricky bit comes in with
     * the replacement. It can be longer, or shorter, than its match (usually
     * shorter). We thus need to take this length difference into account
     * when we replace the string in the original, so that the match offset
     * is correct.
     *
     * While it would be a very nice optimization if we only needed to calculate
     * the replacement string once, the RE could have alternatives which would
     * then change the capture pattern, so we calculate the replacement every
     * time.
     */
    int startpos = 0;
    int need_replacement = 1;
    int slen = nr_strlen(str);
    nr_regex_substrings_t* ss;

    while (NULL
           != (ss = nr_regex_match_capture(rule->regex, str + startpos,
                                           slen - startpos))) {
      int offsets[2];
      int mstart;
      int mend;

      if (NR_FAILURE == nr_regex_substrings_get_offsets(ss, 0, offsets)) {
        nr_regex_substrings_destroy(&ss);
        return NR_RULES_RESULT_UNCHANGED;
      }

      mstart = offsets[0] + startpos;
      mend = offsets[1] + startpos;
      changed++;

      /* Copy before match. */
      wp = nr_strxcpy(wp, str + startpos, mstart - startpos);

      /* Calculate replacement. */
      if (need_replacement) {
        if (rule->rflags & NR_RULE_HAS_CAPTURES) {
          nr_rule_replace_string(rule->replacement, repl, repl_len, ss);
        } else {
          nr_strcpy(repl, rule->replacement);
        }

        need_replacement
            = (rule->rflags & (NR_RULE_HAS_ALTS | NR_RULE_HAS_CAPTURES));
      }

      /* Copy replacement and continue from end of match. */
      wp = nr_strcpy(wp, repl);
      startpos = mend;

      nr_regex_substrings_destroy(&ss);
    }

    /* Copy part after all the matches. */
    nr_strcpy(wp, str + startpos);
    nr_strcpy(str, work);
  } /* End of replace all */

  if (changed) {
    return NR_RULES_RESULT_CHANGED;
  } else {
    return NR_RULES_RESULT_UNCHANGED;
  }
}

nr_rules_result_t nr_rules_apply(const nrrules_t* rules,
                                 const char* name,
                                 char** new_name) {
  int i;
  int changed = 0;
  char* str;
  char* repl;
  char* work;

  if (new_name) {
    *new_name = 0;
  }

  if (nrunlikely((0 == rules) || (0 == name))) {
    return NR_RULES_RESULT_UNCHANGED;
  }

  str = (char*)nr_zalloc(NRULE_BUF_SIZE * 3);
  repl = str + NRULE_BUF_SIZE;
  work = str + (NRULE_BUF_SIZE * 2);

  nr_strlcpy(str, name, NRULE_BUF_SIZE);

  for (i = 0; i < rules->nrules; i++) {
    const nrrule_t* rule = &rules->rules[i];
    int rv = nr_rule_apply(str, work, repl, NRULE_BUF_SIZE, rule);

    if (NR_RULES_RESULT_IGNORE == rv) {
      nr_free(str);
      return NR_RULES_RESULT_IGNORE;
    }

    if (NR_RULES_RESULT_CHANGED == rv) {
      changed++;
      if (rule->rflags & NR_RULE_TERMINATE) {
        break;
      }
    }
  }

  if (changed) {
    if (new_name) {
      *new_name = nr_strdup(str);
    }
    nr_free(str);
    return NR_RULES_RESULT_CHANGED;
  } else {
    nr_free(str);
    return NR_RULES_RESULT_UNCHANGED;
  }
}

void nr_rules_process_rule(nrrules_t* rules, const nrobj_t* rule) {
  uint32_t flags = 0;
  int order;
  const char* mstr;
  const char* rstr;

  if ((0 == rules) || (0 == rule)) {
    return;
  }

  mstr = nro_get_hash_string(rule, "match_expression", 0);
  if (0 == mstr) {
    return;
  }

  if (nr_reply_get_bool(rule, "each_segment", 0)) {
    flags |= NR_RULE_EACH_SEGMENT;
  }

  if (nr_reply_get_bool(rule, "replace_all", 0)) {
    flags |= NR_RULE_REPLACE_ALL;
  }

  if (nr_reply_get_bool(rule, "ignore", 0)) {
    flags |= NR_RULE_IGNORE;
  }

  if (nr_reply_get_bool(rule, "terminate_chain", 0)) {
    flags |= NR_RULE_TERMINATE;
  }

  order = nr_reply_get_int(rule, "eval_order", NR_RULE_DEFAULT_ORDER);

  rstr = nro_get_hash_string(rule, "replacement", 0);
  if ((0 == rstr) && (0 == (flags & NR_RULE_IGNORE))) {
    return;
  }

  if (0 != nr_strchr(mstr, '|')) {
    flags |= NR_RULE_HAS_ALTS;
  }

  if (0 != rstr) {
    char* cp = nr_strchr(rstr, '\\');

    if (0 != cp) {
      if ((cp[1] >= '0') && (cp[1] <= '9')) {
        flags |= NR_RULE_HAS_CAPTURES;
      }
    }
  }

  nr_rules_add(rules, flags, order, mstr, rstr);
}

nrrules_t* nr_rules_create_from_obj(const nrobj_t* obj) {
  int i;
  int nrules;
  nrrules_t* rs;

  if (0 == obj) {
    return 0;
  }
  if (NR_OBJECT_ARRAY != nro_type(obj)) {
    return 0;
  }

  nrules = nro_getsize(obj);
  rs = nr_rules_create(nrules);

  for (i = 0; i < nrules; i++) {
    const nrobj_t* rule = nro_get_array_value(obj, i + 1, 0);

    nr_rules_process_rule(rs, rule);
  }

  nr_rules_sort(rs);

  return rs;
}
