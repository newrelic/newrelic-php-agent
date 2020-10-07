/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_apdex.h"

nr_apdex_zone_t nr_apdex_zone(nrtime_t apdex_threshold, nrtime_t duration) {
  if (duration <= apdex_threshold) {
    return NR_APDEX_SATISFYING;
  } else if (duration <= (4 * apdex_threshold)) {
    return NR_APDEX_TOLERATING;
  }

  return NR_APDEX_FAILING;
}

char nr_apdex_zone_label(nr_apdex_zone_t apdex) {
  switch (apdex) {
    case NR_APDEX_SATISFYING:
      return 'S';

    case NR_APDEX_TOLERATING:
      return 'T';

    case NR_APDEX_FAILING:
      return 'F';

    default:
      return '?';
  }
}
