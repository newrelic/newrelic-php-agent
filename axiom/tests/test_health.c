/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "util_health.h"
#include "util_memory.h"

#include "tlib_main.h"
#include "util_syscalls.h"

static void test_health(void) {
  nr_status_t rv;
  char* location = NULL;

  nrh_set_start_time();

  nr_unlink("health-bc21b5891f5e44fc9272caef924611a8.yml");

  location = nrh_get_health_location("/should/not/exist");
  tlib_pass_if_true("initialization to bad path fails", NULL == location,
                    "location=%s", NULL == location ? "NULL" : location);
  nr_free(location);

  nrh_set_health_filename();

  location = nrh_get_health_location("file://./");
  tlib_pass_if_true("initialization to good path succeeds", NULL != location,
                    "location=%s", NULL == location ? "NULL" : location);

  rv = nrh_set_last_error(NRH_MISSING_LICENSE);

  rv = nrh_write_health(location);
  tlib_pass_if_true("health file write succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);

  tlib_pass_if_exists("./health-bc21b5891f5e44fc9272caef924611a8.yml");

  rv = nrh_set_last_error(NRH_MISSING_APPNAME);

  rv = nrh_write_health(location);
  tlib_pass_if_true("write_health succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);

  rv = nrh_set_last_error(NRH_HEALTHY);

  rv = nrh_write_health(location);
  tlib_pass_if_true("write_health succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);

  nr_free(location);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_health();
}
