/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/time.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "util_health.h"
#include "php_globals.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_logging.h"

#define BILLION (1000000000L)

typedef struct _nrh_status_codes_t {
  const char* code;
  const char* description;
} nrh_status_codes_t;

// clang-format off
static nrh_status_codes_t health_statuses[NRH_MAX_STATUS] = {
  {"NR-APM-000", "Healthy"},
  {"NR-APM-001", "Invalid license key"},
  {"NR-APM-002", "License Key missing in configuration"},
  {"NR-APM-003", "Forced disconnect received from New Relic"},
  {"NR-APM-004", "HTTP error response code [%s] received from New Relic while sending data type [%s]"},
  {"NR-APM-005", "Missing application name in agent configuration"},
  {"NR-APM-006", "The maximum number of configured app names (3) exceeded"},
  {"NR-APM-007", "HTTP Proxy configuration error, response code [%s]"},
  {"NR-APM-008", "Agent is disabled via configuration"},
  {"NR-APM-009", "Failed to connect to New Relic data collector"},
  {"NR-APM-010", "Agent config file is not able to be parsed"},
  {"NR-APM-099", "Agent has shutdown"}
};
// clang-format on

static int healthfile_fd = -1;
static struct timespec start_time = {0, 0};
static nrhealth_t last_error_code = NRH_HEALTHY;
static char health_filename[] = "health-bc21b5891f5e44fc9272caef924611a8.yml";

static char* nrh_get_uuid(void) {
  // TODO: UUID generation logic
  return nr_strdup("bc21b5891f5e44fc9272caef924611a8");
}

char* nrh_strip_scheme_prefix(char* uri) {
  char* filedir = NULL;
  int prefix_len = nr_strlen("file://");
  int uri_len = nr_strlen(uri);

  if (uri_len <= prefix_len) {
    // uri must contain more than just the scheme information.
    nrl_warning(NRL_AGENT, "%s: invalid uri %s", __func__, uri);
    return NULL;
  }

  if (!nr_strstr(uri, "file://")) {
    // missing uri scheme, undefined behavior. treat as error.
    nrl_warning(NRL_AGENT, "%s: invalid uri %s", __func__, uri);
    return NULL;
  }

  // allocate space for stripped string + null terminator.
  filedir = (char*)nr_malloc(uri_len - prefix_len + 1);

  // copy string starting at uri path offset.
  nr_strcpy(filedir, uri + prefix_len);

  return filedir;
}

char* nrh_get_health_filename(void) {
  return health_filename;
}

nr_status_t nrh_set_health_filename(void) {
  char* uuid = NULL;
  char* fname = NULL;

  uuid = nrh_get_uuid();

  if (NULL == uuid) {
    return NR_FAILURE;
  }

  fname = nr_formatf("health-%s.yml", uuid);

  if (nr_strlen(fname) != nr_strlen(health_filename)) {
    nr_free(uuid);
    return NR_FAILURE;
  }

  nr_strcpy(&health_filename[0], fname);

  nr_free(uuid);
  nr_free(fname);
  return NR_SUCCESS;
}

char* nrh_get_health_location(char* uri) {
  char* filedir = NULL;
  struct stat statbuf;

  if (NULL == uri || 0 == uri[0]) {
    return NULL;
  }

  filedir = nrh_strip_scheme_prefix(uri);

  if (NULL == filedir) {
    return NULL;
  }

  if (0 != nr_stat(filedir, &statbuf)) {
    nr_free(filedir);
    return NULL;
  }

  if (0 == S_ISDIR(statbuf.st_mode)) {
    nr_free(filedir);
    return NULL;
  }

  return filedir;
}

char* nrh_get_health_filepath(char* filedir) {
  char* filepath = NULL;

  if (NULL == filedir) {
    return NULL;
  }

  filepath = nr_formatf("%s/%s", filedir, health_filename);

  return filepath;
}

void nrh_close_health_file(void) {
  if (-1 == healthfile_fd) {
    return;
  }

  nr_close(healthfile_fd);
  healthfile_fd = -1;
}

nr_status_t nrh_set_start_time(void) {
  clock_gettime(CLOCK_REALTIME, &start_time);

  if (0 == start_time.tv_nsec) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

long long nrh_get_start_time_ns(void) {
  return (long long)(start_time.tv_sec * BILLION + start_time.tv_nsec);
}

long long nrh_get_current_time_ns(void) {
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);

  return (long long)(ts.tv_sec * BILLION + ts.tv_nsec);
}

int nrh_get_healthfile_fd(void) {
  return healthfile_fd;
}

nr_status_t nrh_set_last_error(nrhealth_t status) {
  if (status < NRH_HEALTHY || status >= NRH_MAX_STATUS) {
    return NR_FAILURE;
  }

  if (NRH_SHUTDOWN == status && NRH_HEALTHY != last_error_code) {
    // cannot report shutdown if agent is unhealthy
    return NR_FAILURE;
  }

  last_error_code = status;
  return NR_SUCCESS;
}

nrhealth_t nrh_get_last_error(void) {
  return last_error_code;
}

#ifndef HEALTH_STATUS_LINE
#define HEALTH_STATUS_LINE(field, value)                        \
  nr_write(healthfile_fd, nr_formatf("%s: %s\n", field, value), \
           nr_strlen(nr_formatf("%s: %s\n", field, value)));

nr_status_t nrh_write_health(void) {
  nrhealth_t status = last_error_code;

  if (-1 == healthfile_fd) {
    // healthfile not initialized
    return NR_FAILURE;
  }

  HEALTH_STATUS_LINE("healthy", NRH_HEALTHY == status ? "true" : "false");

  HEALTH_STATUS_LINE("status", health_statuses[status].description);

  HEALTH_STATUS_LINE("last_error_code", health_statuses[status].code);

  HEALTH_STATUS_LINE("status_time_unix_nano",
                     nr_formatf("%lld", nrh_get_current_time_ns()));

  HEALTH_STATUS_LINE("start_time_unix_nano",
                     nr_formatf("%lld", nrh_get_start_time_ns()));

  return NR_SUCCESS;
}
#undef HEALTH_STATUS_LINE
#endif
