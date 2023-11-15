/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_matcher.h"
#include "util_matcher_private.h"

#include "util_memory.h"
#include "util_strings.h"

static void nr_matcher_prefix_dtor(void* prefix, void* userdata NRUNUSED) {
  nr_free(prefix);
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

bool nr_matcher_add_prefix(nr_matcher_t* matcher, const char* prefix) {
  size_t i;
  char* prefix_lc;
  size_t prefix_len;

  if (NULL == matcher || NULL == prefix) {
    return false;
  }

  prefix_len = nr_strlen(prefix);
  while (prefix_len > 0 && '/' == prefix[prefix_len - 1]) {
    prefix_len--;
  }

  prefix_lc = nr_malloc(prefix_len + 2);
  for (i = 0; i < prefix_len; i++) {
    prefix_lc[i] = nr_tolower(prefix[i]);
  }
  prefix_lc[prefix_len] = '/';
  prefix_lc[prefix_len + 1] = '\0';

  return nr_vector_push_back(&matcher->prefixes, prefix_lc);
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
    const char* prefix = nr_vector_get(&matcher->prefixes, i);

    found = nr_strstr(input, prefix);
    if (found) {
      const char* slash;

      found += nr_strlen(prefix);
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
