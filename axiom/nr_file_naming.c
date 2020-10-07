/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides transaction file naming support.
 */
#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "nr_file_naming.h"
#include "nr_file_naming_private.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

static void nr_file_namer_destroy_internal(nr_file_naming_t* namer) {
  nr_regex_destroy(&namer->regex);
  nr_free(namer->user_pattern);
  nr_free(namer);
}

void nr_file_namer_destroy(nr_file_naming_t** namer_ptr) {
  nr_file_naming_t* namer;

  if (NULL == namer_ptr) {
    return;
  }

  namer = *namer_ptr;

  while (namer) {
    nr_file_naming_t* next = namer->next;

    nr_file_namer_destroy_internal(namer);
    namer = next;
  }

  *namer_ptr = NULL;
}

static char* nr_file_namer_match_one(const nr_file_naming_t* namer,
                                     const char* filename) {
  nr_regex_substrings_t* ss;
  char* s;

  ss = nr_regex_match_capture(namer->regex, filename, nr_strlen(filename));

  if (nr_regex_substrings_count(ss) < 1) {
    nr_regex_substrings_destroy(&ss);
    return NULL;
  }

  s = nr_regex_substrings_get(ss, 1);
  nr_regex_substrings_destroy(&ss);

  if (NULL == s) {
    nrl_error(NRL_AGENT,
              "unexpected NULL substring for filename=" NRP_FMT
              " pattern=" NRP_FMT,
              NRP_FILENAME(filename), NRP_PHP(namer->user_pattern));
  }

  return s;
}

char* nr_file_namer_match(const nr_file_naming_t* namer, const char* filename) {
  if ((NULL == filename) || ('\0' == filename[0])) {
    return NULL;
  }

  for (; namer; namer = namer->next) {
    char* rval;

    rval = nr_file_namer_match_one(namer, filename);
    if (rval) {
      return rval;
    }
  }

  return NULL;
}

static const int nr_file_naming_regex_options
    = NR_REGEX_CASELESS | NR_REGEX_DOLLAR_ENDONLY | NR_REGEX_DOTALL;

static nr_file_naming_t* nr_file_namer_create(const char* user_pattern) {
  int len;
  nr_file_naming_t* new_namer;
  char* regex_pattern = NULL;

  if ((NULL == user_pattern) || ('\0' == user_pattern[0])) {
    return NULL;
  }

  new_namer = (nr_file_naming_t*)nr_zalloc(sizeof(nr_file_naming_t));

  new_namer->user_pattern = nr_strdup(user_pattern);

  len = nr_strlen(user_pattern);

  /*
   * In keeping with the old behavior of this function, paths ending in a slash
   * may be followed by any number of literal periods.
   *
   * This is preserved historical behavior.
   */

  if ('/' == user_pattern[len - 1]) {
    regex_pattern = nr_formatf(".*(%s\\.*)", user_pattern);
  } else {
    regex_pattern = nr_formatf(".*(%s)", user_pattern);
  }

  new_namer->regex
      = nr_regex_create(regex_pattern, nr_file_naming_regex_options, 1);

  nr_free(regex_pattern);

  if (NULL == new_namer->regex) {
    nrl_error(NRL_INSTRUMENT,
              "invalid regular expression pattern used in the value of "
              "transaction file namer" NRP_FMT,
              NRP_PHP(user_pattern));

    nr_file_namer_destroy(&new_namer);
    return NULL;
  }

  return new_namer;
}

nr_file_naming_t* nr_file_namer_append(nr_file_naming_t* curr_head,
                                       const char* user_pattern) {
  nr_file_naming_t* new_namer = nr_file_namer_create(user_pattern);

  if (NULL == new_namer) {
    return curr_head;
  }

  new_namer->next = curr_head;
  return new_namer;
}
