/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_FILE_NAMING_PRIVATE_HDR
#define NR_FILE_NAMING_PRIVATE_HDR

#include "util_regex.h"

struct _nr_file_naming_t {
  struct _nr_file_naming_t* next; /* singly linked list next pointer */
  nr_regex_t* regex;              /* Regex to match file names */
  char* user_pattern;
};

#endif /* NR_FILE_NAMING_PRIVATE_HDR */
