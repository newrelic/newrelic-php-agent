/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_matcher.h"
#include "util_matcher_private.h"

#include "util_memory.h"
#include "util_strings.h"

typedef struct {
  char *cp;
  int len;
} matcher_prefix;

static void nr_matcher_prefix_dtor(void* _p, void* userdata NRUNUSED) {
  matcher_prefix* p = (matcher_prefix *)_p;
  nr_free(p->cp);
  nr_free(p);
}

nr_matcher_t* nr_matcher_create(void) {
  nr_matcher_t* matcher;

  matcher = nr_malloc(sizeof(nr_matcher_t));
  nr_vector_init(&matcher->prefixes, 0, nr_matcher_prefix_dtor, NULL);

  return matcher;
}

void nr_matcher_destroy(nr_matcher_t** matcher_ptr) {
  if (NULL == matcher_ptr || NULL == *matcher_ptr) {
    return;
  }

  nr_vector_deinit(&((*matcher_ptr)->prefixes));
  nr_realfree((void**)matcher_ptr);
}

bool nr_matcher_add_prefix(nr_matcher_t* matcher, const char* str) {
  int i;
  matcher_prefix* prefix;

  if (NULL == matcher || NULL == str) {
    return false;
  }

  prefix = nr_calloc(1, sizeof(matcher_prefix));
  prefix->len = nr_strlen(str);
  while (prefix->len > 0 && '/' == str[prefix->len - 1]) {
    prefix->len--;
  }

  prefix->len += 1;
  prefix->cp = nr_malloc(prefix->len+1);
  for (i = 0; i < prefix->len; i++) {
    prefix->cp[i] = nr_tolower(str[i]);
  }
  prefix->cp[prefix->len-1] = '/';
  prefix->cp[prefix->len] = '\0';

  return nr_vector_push_back(&matcher->prefixes, prefix);
}
static char* nr_matcher_match_internal(nr_matcher_t* matcher,
                                       const char* input,
                                       bool core) {
  size_t i;
  char* input_lc;
  char* match = NULL;
  size_t num_prefixes;

  if (NULL == matcher || NULL == input) {
    return NULL;
  }

  num_prefixes = nr_vector_size(&matcher->prefixes);
  input_lc = nr_string_to_lowercase(input);

  for (i = 0; i < num_prefixes; i++) {
    const char* found;
    const matcher_prefix* prefix = nr_vector_get(&matcher->prefixes, i);

    found = nr_strstr(input, prefix->cp);
    if (found) {
      const char* slash;

      found += prefix->len;
      if (true == core) {
        slash = nr_strrchr(found, '/');
      } else {
        slash = nr_strchr(found, '/');
      }
      if (NULL == slash) {
        match = nr_strdup(found);
      } else {
        if (true == core) {
          const char* offset = input + nr_strlen(input);
          match = nr_strndup(slash + 1, offset - slash);
        } else {
          match = nr_strndup(found, slash - found);
        }
      }
      break;
    }
  }

  nr_free(input_lc);
  return match;
}

char* nr_matcher_match(nr_matcher_t* matcher, const char* input) {
  return nr_matcher_match_internal(matcher, input, false);
}

char* nr_matcher_match_core(nr_matcher_t* matcher, const char* input) {
  return nr_matcher_match_internal(matcher, input, true);
}
