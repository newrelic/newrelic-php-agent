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

  if (NULL == (prefix = nr_calloc(1, sizeof(matcher_prefix)))) {
    return false;
  }
  prefix->len = nr_strlen(str);
  while (prefix->len > 0 && '/' == str[prefix->len - 1]) {
    prefix->len--;
  }

  prefix->len += 1; // +1 for the trailing '/'
  if (NULL == (prefix->cp = nr_malloc(prefix->len+1))) { // +1 for the '\0'
    nr_matcher_prefix_dtor(prefix, NULL);
    return false;
  }
  for (i = 0; i < prefix->len; i++) {
    prefix->cp[i] = nr_tolower(str[i]);
  }
  prefix->cp[prefix->len-1] = '/';
  prefix->cp[prefix->len] = '\0';

  return nr_vector_push_back(&matcher->prefixes, prefix);
}

#define SET_SAFE(p, v) do {\
  if (NULL != p) *p = (v); \
} while(0);
static char* nr_matcher_match_internal(nr_matcher_t* matcher, 
                                       const char* input,
                                       int input_len,
                                       int* match_len,
                                       bool core) {
  size_t i;
  char* input_lc;
  char* match = NULL;
  size_t num_prefixes;

  SET_SAFE(match_len, 0);

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
        SET_SAFE(match_len, input_len - (found-input));
      } else {
        if (true == core) {
          const char* offset = input + input_len;
          match = nr_strndup(slash + 1, offset - slash);
          SET_SAFE(match_len, offset - (slash+1));
        } else {
          match = nr_strndup(found, slash - found);
          SET_SAFE(match_len, slash - found);
        }
      }
      break;
    }
  }

  nr_free(input_lc);
  return match;
}

char* nr_matcher_match_ex(nr_matcher_t* matcher, const char* input, int input_len, int *match_len) {
  return nr_matcher_match_internal(matcher, input, input_len, match_len, false);
}

char* nr_matcher_match(nr_matcher_t* matcher, const char* input) {
  return nr_matcher_match_internal(matcher, input, nr_strlen(input), NULL, false);
}

char* nr_matcher_match_core_ex(nr_matcher_t* matcher, const char* input, int input_len, int *match_len) {
  return nr_matcher_match_internal(matcher, input, input_len, match_len, true);
}

char* nr_matcher_match_core(nr_matcher_t* matcher, const char* input) {
  return nr_matcher_match_internal(matcher, input, nr_strlen(input), NULL, true);
}
