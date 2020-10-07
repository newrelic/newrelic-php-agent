/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains function for parsing and formatting labels.
 */
#ifndef UTIL_LABELS_HDR
#define UTIL_LABELS_HDR

#include "util_object.h"

#define NR_LABEL_PAIR_LIMIT 64
#define NR_LABEL_KEY_LENGTH_MAX 255
#define NR_LABEL_VALUE_LENGTH_MAX 255

/*
 * Purpose : Convert a string representation of label name/value pairs into
 *           a nrobj_t hash.
 *
 * Params  : 1. The string to parse.
 *
 * Returns the nrobj_t hash representation of the input string.
 * Returns null if the input string is null or empty.
 *
 */
extern nrobj_t* nr_labels_parse(const char* str);

extern nrobj_t* nr_labels_connector_format(const nrobj_t* object);

#endif /* UTIL_LABELS_HDR */
