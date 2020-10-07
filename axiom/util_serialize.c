/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with data serialised with PHP's
 * serialize() function.
 */
#include "nr_axiom.h"

#include <stddef.h>

#include "util_regex.h"
#include "util_serialize.h"
#include "util_strings.h"

char* nr_serialize_get_class_name(const char* data, int data_len) {
  char* name;
  nr_regex_t* regex;
  nr_regex_substrings_t* ss;

  if ((NULL == data) || (0 == data_len)) {
    return NULL;
  }

  regex = nr_regex_create(
      "O:\\d+:\"([a-zA-Z_\\x7f-\\xff][a-zA-Z0-9_\\x7f-\\xff\\\\]*)\":",
      NR_REGEX_ANCHORED, 0);
  if (NULL == regex) {
    return NULL;
  }

  ss = nr_regex_match_capture(regex, data, data_len);
  if (NULL == ss) {
    nr_regex_destroy(&regex);
    return NULL;
  }

  name = nr_regex_substrings_get(ss, 1);

  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);

  return name;
}
