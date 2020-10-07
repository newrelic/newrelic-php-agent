/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Routines to parse labels.
 */
#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "util_labels.h"
#include "util_memory.h"
#include "util_number_converter.h"
#include "util_strings.h"

static int nr_labels_get_whitespace_prefix_len(const char* input) {
  int i;

  if ((NULL == input) || (0 == input[0])) {
    return 0;
  }

  i = 0;

  while (nr_isspace(input[i])) {
    i++;
  }

  return i;
}

static int nr_labels_get_whitespace_suffix_len(const char* input,
                                               int len_to_test) {
  const char* current;
  const char* end;

  if ((NULL == input) || (0 == input[0]) || (len_to_test <= 0)) {
    return 0;
  }

  end = input + len_to_test - 1;
  current = end;
  while ((current >= input) && (0 != *current) && nr_isspace(*current)) {
    current--;
  }

  return (end - current);
}

static nrobj_t* nr_labels_parse_internal(const char* str) {
  nrobj_t* labels = 0;
  int count = 0;
  int i;
  nrobj_t* split_str = 0;
  char key_buf[NR_LABEL_KEY_LENGTH_MAX + 1];
  char val_buf[NR_LABEL_VALUE_LENGTH_MAX + 1];
  const char* raw;
  int raw_len;
  int ws_prefix_len;
  int ws_suffix_len;
  int trimmed_len;

  if ((NULL == str) || (0 == str[0])) {
    return NULL;
  }

  split_str = nr_strsplit(str, ";", 1);
  count = nro_getsize(split_str);
  if (NULL == split_str) {
    return NULL;
  }

  if (count > NR_LABEL_PAIR_LIMIT) {
    count = NR_LABEL_PAIR_LIMIT;
  }

  labels = nro_new_hash();

  for (i = 0; i < count; i++) {
    const char* pair = nro_get_array_string(split_str, i + 1, NULL);
    const char* colon = nr_strchr(pair, ':');

    if (NULL == colon) {
      goto leave;
    }

    key_buf[0] = 0;
    val_buf[0] = 0;

    raw = pair;
    raw_len = colon - pair;
    ws_prefix_len = nr_labels_get_whitespace_prefix_len(raw);
    ws_suffix_len = nr_labels_get_whitespace_suffix_len(raw, raw_len);
    trimmed_len = raw_len - ws_prefix_len - ws_suffix_len;
    if (trimmed_len <= 0) {
      goto leave;
    }
    snprintf(key_buf, sizeof(key_buf), "%.*s", trimmed_len,
             raw + ws_prefix_len);
    if (NULL != nr_strchr(key_buf, ':')) {
      goto leave;
    }

    raw = colon + 1;
    raw_len = nr_strlen(colon + 1);
    ws_prefix_len = nr_labels_get_whitespace_prefix_len(raw);
    ws_suffix_len = nr_labels_get_whitespace_suffix_len(raw, raw_len);
    trimmed_len = raw_len - ws_prefix_len - ws_suffix_len;
    if (trimmed_len <= 0) {
      goto leave;
    }
    snprintf(val_buf, sizeof(val_buf), "%.*s", trimmed_len,
             raw + ws_prefix_len);
    if (NULL != nr_strchr(val_buf, ':')) {
      goto leave;
    }

    nro_set_hash_string(labels, key_buf, val_buf);
  }

  nro_delete(split_str);

  return labels;

leave:
  nro_delete(split_str);
  nro_delete(labels);
  return NULL;
}

static char* nr_labels_strip_surrounding_semicolons_whitespace(
    const char* str) {
  int i;
  char* dup;
  int len;

  if (NULL == str) {
    return NULL;
  }

  while ((';' == *str) || nr_isspace(*str)) {
    str++;
  }

  dup = nr_strdup(str);
  len = nr_strlen(str);

  for (i = len - 1; i >= 0; i--) {
    if ((';' == dup[i]) || nr_isspace(dup[i])) {
      dup[i] = '\0';
    } else {
      break;
    }
  }

  return dup;
}

nrobj_t* nr_labels_parse(const char* str) {
  char* dup;
  nrobj_t* obj;

  if (NULL == str) {
    return NULL;
  }

  dup = nr_labels_strip_surrounding_semicolons_whitespace(str);

  obj = nr_labels_parse_internal(dup);

  nr_free(dup);

  return obj;
}

static nr_status_t nr_labels_connector_format_iterator(const char* key,
                                                       const nrobj_t* val,
                                                       void* ptr) {
  nrobj_t* arr = (nrobj_t*)ptr;
  nrobj_t* hash = nro_new_hash();

  nro_set_hash_string(hash, "label_type", key);
  nro_set_hash_string(hash, "label_value", nro_get_string(val, NULL));

  nro_set_array(arr, 0, hash);
  nro_delete(hash);

  return NR_SUCCESS;
}

nrobj_t* nr_labels_connector_format(const nrobj_t* object) {
  nrobj_t* arr = nro_new_array();

  nro_iteratehash(object, nr_labels_connector_format_iterator, arr);

  return arr;
}
