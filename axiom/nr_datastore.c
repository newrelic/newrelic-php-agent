/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_datastore.h"
#include "nr_datastore_private.h"
#include "util_memory.h"
#include "util_strings.h"

const char* nr_datastore_as_string(nr_datastore_t ds) {
  size_t i;

  for (i = 0; i < datastore_mappings_len; i++) {
    if (ds == datastore_mappings[i].datastore) {
      return datastore_mappings[i].str;
    }
  }

  return NULL;
}

nr_datastore_t nr_datastore_from_string(const char* str) {
  nr_datastore_t ds = NR_DATASTORE_OTHER;
  size_t i;
  char* lcstr;

  if (NULL == str) {
    return ds;
  }

  lcstr = nr_string_to_lowercase(str);
  for (i = 0; i < datastore_mappings_len; i++) {
    const char* expected = datastore_mappings[i].lowercase;

    if ((NULL != expected) && (0 == nr_strcmp(expected, lcstr))) {
      ds = datastore_mappings[i].datastore;
      break;
    }
  }

  nr_free(lcstr);
  return ds;
}

int nr_datastore_is_sql(nr_datastore_t ds) {
  size_t i;

  for (i = 0; i < datastore_mappings_len; i++) {
    if (ds == datastore_mappings[i].datastore) {
      return datastore_mappings[i].is_sql;
    }
  }

  return 0;
}
