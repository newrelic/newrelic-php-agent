/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains support for tests that use application structures.
 */
#ifndef TEST_APP_HELPERS_HDR
#define TEST_APP_HELPERS_HDR

#include "nr_app.h"
#include "nr_limits.h"

static inline nr_app_limits_t default_app_limits(void) {
  return (nr_app_limits_t){
      .analytics_events = NR_MAX_ANALYTIC_EVENTS,
      .custom_events = NR_MAX_CUSTOM_EVENTS,
      .error_events = NR_MAX_ERRORS,
      .span_events = NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED,
      .log_events = NR_DEFAULT_LOG_EVENTS_MAX_SAMPLES_STORED,
  };
}

#endif /* TEST_APP_HELPERS_HDR */
