/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains data types and functions for dealing with apdex.
 *
 * The apdex specification:
 * http://apdex.org/documents/ApdexTechnicalSpecificationV11_000.pdf
 */
#ifndef UTIL_APDEX_HDR
#define UTIL_APDEX_HDR

#include "util_time.h"

/*
 * A type enumerating the possible apdex zones.
 */
typedef enum _nr_apdex_zone_t {
  NR_APDEX_SATISFYING = 1,
  NR_APDEX_TOLERATING,
  NR_APDEX_FAILING,
} nr_apdex_zone_t;

/*
 * Purpose : Calculate the apdex zone for the given duration.
 *
 * Params  : 1. The apdex T value.
 *           2. The transaction duration.
 *
 * Returns : The apdex zone.
 */
extern nr_apdex_zone_t nr_apdex_zone(nrtime_t apdex_threshold,
                                     nrtime_t duration);

/*
 * Purpose : Return the label for the given apdex zone.
 *
 * Params  : 1. The apdex zone.
 *
 * Returns : The single character representing the zone.
 */
extern char nr_apdex_zone_label(nr_apdex_zone_t apdex);

#endif /* UTIL_APDEX_HDR */
