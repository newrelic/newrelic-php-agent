/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for agent control health file handling.
 */
#ifndef UTIL_HEALTH_HDR
#define UTIL_HEALTH_HDR

#include <sys/time.h>

#include "nr_axiom.h"

typedef enum _nrhealth_t {
  NRH_HEALTHY = 0,
  NRH_INVALID_LICENSE,
  NRH_MISSING_LICENSE,
  NRH_FORCED_DISCONNECT,
  NRH_HTTP_ERROR,
  NRH_MISSING_APPNAME,
  NRH_MAX_APPNAME,
  NRH_PROXY_ERROR,
  NRH_AGENT_DISABLED,
  NRH_CONNECTION_FAILED,
  NRH_CONFIG_ERROR,
  NRH_SHUTDOWN,
  NRH_MAX_STATUS
} nrhealth_t;

/* utility */
extern char* nrh_strip_scheme_prefix(char* uri);
extern nr_status_t nrh_write_health(char* uri);
extern char* nrh_generate_uuid(void);

/* getters */
extern char* nrh_get_health_location(char* uri);
extern char* nrh_get_health_filepath(char* filedir);
extern char* nrh_get_health_filename(void);
extern long long nrh_get_start_time_ns(void);
extern long long nrh_get_current_time_ns(void);
extern nrhealth_t nrh_get_last_error(void);

/* setters */
extern nr_status_t nrh_set_start_time(void);
extern nr_status_t nrh_set_last_error(nrhealth_t code);
extern nr_status_t nrh_set_uuid(char* uuid);

#endif /* UTIL_HEALTH_HDR */
