/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_segment_terms.h"
#include "nr_segment_terms_private.h"
#include "util_buffer.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"

nr_segment_terms_t* nr_segment_terms_create(int size) {
  nr_segment_terms_t* terms = NULL;

  if (0 >= size) {
    return NULL;
  }

  terms = (nr_segment_terms_t*)nr_malloc(sizeof(nr_segment_terms_t));
  terms->rules = (nr_segment_terms_rule_t**)nr_calloc(
      size, sizeof(nr_segment_terms_rule_t));
  terms->capacity = size;
  terms->size = 0;

  return terms;
}

nr_segment_terms_t* nr_segment_terms_create_from_obj(const nrobj_t* obj) {
  int i;
  int num_terms;
  nr_segment_terms_t* terms;

  if ((NULL == obj) || (NR_OBJECT_ARRAY != nro_type(obj))) {
    return NULL;
  }

  num_terms = nro_getsize(obj);
  terms = nr_segment_terms_create(num_terms);
  if (NULL == terms) {
    return NULL;
  }

  for (i = 1; i <= num_terms; i++) {
    if (NR_FAILURE
        == nr_segment_terms_add_from_obj(terms,
                                         nro_get_array_hash(obj, i, NULL))) {
      nr_segment_terms_destroy(&terms);
      return NULL;
    }
  }

  return terms;
}

void nr_segment_terms_destroy(nr_segment_terms_t** terms_ptr) {
  nr_segment_terms_t* terms;
  int i;

  if ((NULL == terms_ptr) || (NULL == *terms_ptr)) {
    return;
  }

  terms = *terms_ptr;
  for (i = 0; i < terms->size; i++) {
    nr_segment_terms_rule_destroy(&terms->rules[i]);
  }

  nr_free(terms->rules);
  nr_realfree((void**)terms_ptr);
}

nr_status_t nr_segment_terms_add(nr_segment_terms_t* segment_terms,
                                 const char* prefix,
                                 const nrobj_t* terms) {
  nr_segment_terms_rule_t* rule;

  /*
   * We'll check the other parameters in nr_segment_terms_rule_create.
   */
  if (NULL == segment_terms) {
    return NR_FAILURE;
  }

  if (segment_terms->size >= segment_terms->capacity) {
    return NR_FAILURE;
  }

  rule = nr_segment_terms_rule_create(prefix, terms);
  if (NULL == rule) {
    return NR_FAILURE;
  }

  segment_terms->rules[segment_terms->size] = rule;
  segment_terms->size++;

  return NR_SUCCESS;
}

nr_status_t nr_segment_terms_add_from_obj(nr_segment_terms_t* segment_terms,
                                          const nrobj_t* rule) {
  const char* prefix;
  const nrobj_t* terms;

  if ((NULL == segment_terms) || (NULL == rule)
      || (NR_OBJECT_HASH != nro_type(rule))) {
    return NR_FAILURE;
  }

  prefix = nro_get_hash_string(rule, "prefix", NULL);
  terms = nro_get_hash_array(rule, "terms", NULL);

  return nr_segment_terms_add(segment_terms, prefix, terms);
}

char* nr_segment_terms_apply(const nr_segment_terms_t* segment_terms,
                             const char* name) {
  char* result = NULL;
  int i;

  if ((NULL == segment_terms) || (NULL == name) || ('\0' == *name)) {
    return NULL;
  }

  /*
   * Per the spec, we apply in reverse order, as the last rule should win.
   */
  for (i = segment_terms->size - 1; i >= 0; i--) {
    result = nr_segment_terms_rule_apply(segment_terms->rules[i], name);
    if (NULL != result) {
      return result;
    }
  }

  return nr_strdup(name);
}

nr_segment_terms_rule_t* nr_segment_terms_rule_create(const char* prefix,
                                                      const nrobj_t* terms) {
  char* regex = NULL;
  nr_segment_terms_rule_t* rule = NULL;

  if ((NULL == prefix) || ('\0' == *prefix)) {
    return NULL;
  }

  rule = (nr_segment_terms_rule_t*)nr_zalloc(sizeof(nr_segment_terms_rule_t));

  /*
   * Since we can only ever match complete segments, we should add a trailing /
   * to the prefix if there isn't one already.
   */
  rule->prefix_len = nr_strlen(prefix);
  if ('/' == prefix[rule->prefix_len - 1]) {
    rule->prefix = nr_strdup(prefix);
  } else {
    rule->prefix = nr_formatf("%s/", prefix);
    rule->prefix_len = nr_strlen(rule->prefix);
  }

  /*
   * We expect exactly two segments in the prefix, which means that we should
   * have two instances of / characters, since we added a trailing slash if
   * needed above.
   */
  if (2 != nr_str_char_count(rule->prefix, '/')) {
    nr_segment_terms_rule_destroy(&rule);
    goto end;
  }

  /*
   * Build up a buffer with a regular expression that will match name segments
   * that aren't part of our list of terms.
   */
  regex = nr_segment_terms_rule_build_regex(terms);
  if (NULL == regex) {
    nr_segment_terms_rule_destroy(&rule);
    goto end;
  }

  /*
   * Compile the regular expression.
   */
  rule->re = nr_regex_create(regex, NR_REGEX_ANCHORED | NR_REGEX_CASELESS, 1);
  if (NULL == rule->re) {
    nr_segment_terms_rule_destroy(&rule);
    goto end;
  }

  /* Fall through to ensure correct destruction. */

end:
  nr_free(regex);

  return rule;
}

void nr_segment_terms_rule_destroy(nr_segment_terms_rule_t** rule_ptr) {
  nr_segment_terms_rule_t* rule;

  if ((NULL == rule_ptr) || (NULL == *rule_ptr)) {
    return;
  }

  rule = *rule_ptr;

  nr_free(rule->prefix);
  nr_regex_destroy(&rule->re);

  nr_realfree((void**)rule_ptr);
}

char* nr_segment_terms_rule_apply(const nr_segment_terms_rule_t* rule,
                                  const char* name) {
  nrbuf_t* buf;
  int i;
  int name_len;
  int previous_replaced = 0;
  int segment_count = 0;
  nrobj_t* segments = NULL;
  char* transformed = NULL;

  if ((NULL == rule) || (NULL == name) || ('\0' == *name)) {
    return NULL;
  }

  /*
   * Short circuit short names: if the name is shorter than the prefix, then
   * obviously it can't be a match. (Plus, we need the name length later
   * anyway.)
   */
  name_len = nr_strlen(name);
  if (name_len < rule->prefix_len) {
    return NULL;
  }

  /*
   * The segment terms spec describes the algorithm that we need to apply here.
   *
   * Firstly, we obviously check if the rule prefix matches the name. If it
   * doesn't, then the rule doesn't match, and we don't need to apply it.
   */
  if (0 != nr_strnicmp(name, rule->prefix, rule->prefix_len)) {
    return NULL;
  }

  /*
   * If there's nothing after the prefix, then we don't have to do anything,
   * and can just return a copy of the transaction name.
   */
  if ('\0' == name[rule->prefix_len]) {
    return nr_strdup(name);
  }

  /*
   * We now split the remainder of the name into segments.
   */
  segments = nr_strsplit(name + rule->prefix_len, "/", 1);
  segment_count = nro_getsize(segments);
  if (NULL == segments) {
    return NULL;
  }

  /*
   * We want to iterate over the segments and apply the rule regex to it. If
   * the regex matches, then the segment matches one of the whitelisted terms,
   * and should be preserved. If the regex doesn't match, then we should
   * replace the segment with the placeholder '*'.
   *
   * previous_replaced is used to implement the collapsing behaviour in the
   * spec: we don't want consecutive * segments, so we track if we just
   * inserted one and don't insert more until we insert a normal segment.
   *
   * requires_delimiter helps with slash insertion: we generally want to
   * prepend a slash before adding a segment, but have already handled any
   * leading slash immediately after the prefix. This is simply a friendlier
   * name for "is this NOT the first segment we're inserting".
   */
  buf = nr_buffer_create(name_len, 0);
  nr_buffer_add(buf, name, rule->prefix_len);
  previous_replaced = 0;
  for (i = 0; i < segment_count; i++) {
    nr_status_t matched = NR_FAILURE;
    const char* s = nro_get_array_string(segments, i + 1, NULL);
    int segment_len = nr_strlen(s);
    int requires_delimiter = (i > 0);

    /*
     * Short circuit empty segments, since they can never match.
     */
    if (segment_len > 0) {
      matched = nr_regex_match(rule->re, s, segment_len);
    }

    if (NR_FAILURE == matched) {
      if (!previous_replaced) {
        if (requires_delimiter) {
          nr_buffer_add(buf, NR_PSTR("/"));
        }
        nr_buffer_add(buf, NR_PSTR("*"));
      }
      previous_replaced = 1;
      continue;
    }

    if (requires_delimiter) {
      nr_buffer_add(buf, NR_PSTR("/"));
    }
    nr_buffer_add(buf, s, nr_strlen(s));
    previous_replaced = 0;
  }

  nr_buffer_add(buf, NR_PSTR("\0"));
  transformed = nr_strdup((const char*)nr_buffer_cptr(buf));

  nr_buffer_destroy(&buf);
  nro_delete(segments);

  return transformed;
}

char* nr_segment_terms_rule_build_regex(const nrobj_t* terms) {
  nrbuf_t* buf;
  char* regex;
  int regex_len;
  int i;
  int terms_len;

  if ((NULL == terms) || (NR_OBJECT_ARRAY != nro_type(terms))) {
    return NULL;
  }

  /*
   * If there aren't any terms, then the expected behaviour is not to match
   * anything. We'll return a regex that can't possibly match anything.
   */
  terms_len = nro_getsize(terms);
  if (0 == terms_len) {
    return nr_strdup("$.");
  }

  /*
   * We want to build up a regex that, for terms of ["a", "b", "c"], looks like
   * this:
   *
   * (a)|(b)|(c)
   */
  buf = nr_buffer_create(0, 0);
  for (i = 0; i < terms_len; i++) {
    const char* term;
    int term_len;

    term = nro_get_array_string(terms, i + 1, NULL);
    if (NULL == term) {
      continue;
    }

    term_len = nr_strlen(term);
    if (0 == term_len) {
      continue;
    }

    if (0 == i) {
      nr_buffer_add(buf, NR_PSTR("("));
    } else {
      nr_buffer_add(buf, NR_PSTR("|("));
    }

    nr_regex_add_quoted_to_buffer(buf, term, term_len);
    nr_buffer_add(buf, NR_PSTR(")"));
  }

  /*
   * Ensure we have a null terminator, since the PCRE API expects the regex to
   * be terminated.
   */
  nr_buffer_add(buf, NR_PSTR("\0"));

  /*
   * Use the data within the buffer for our return value and clean up.
   */
  regex_len = nr_buffer_len(buf);
  regex = (char*)nr_malloc(regex_len);
  (void)nr_buffer_use(buf, (void*)regex, regex_len);
  nr_buffer_destroy(&buf);

  return regex;
}
