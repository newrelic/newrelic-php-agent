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
#include "util_strings.h"
#include "util_syscalls.h"

static void test_health(void) {
  nr_status_t rv;
  char* location = NULL;
  char* rand_uuid = NULL;
  char* rand_healthfile = NULL;

  nrh_set_start_time();

  // ensure clean environment
  nr_unlink("health-bc21b5891f5e44fc9272caef924611a8.yml");
  nr_unlink("health-ffffffffffffffffffffffffffffffff.yml");

  // invalid location
  location = nrh_get_health_location("/should/not/exist");
  tlib_pass_if_true("initialization to bad path fails", NULL == location,
                    "location=%s", NULL == location ? "NULL" : location);
  nr_free(location);

  // invalid uuid
  rv = nrh_set_uuid("bc21b5891f5e44fc9272caef924611a");
  tlib_pass_if_true("set uuid with invalid length uuid fails", NR_FAILURE == rv,
                    "rv=%d", (int)rv);

  // valid location
  location = nrh_get_health_location("file://./");
  tlib_pass_if_true("initialization to good path succeeds", NULL != location,
                    "location=%s", NULL == location ? "NULL" : location);

  // valid status
  rv = nrh_set_last_error(NRH_INVALID_LICENSE);

  // write default uuid + valid location + valid status
  rv = nrh_write_health(location);
  tlib_pass_if_true("health file write succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_exists("./health-bc21b5891f5e44fc9272caef924611a8.yml");

  // update to a new uuid
  rv = nrh_set_uuid("ffffffffffffffffffffffffffffffff");
  tlib_pass_if_true("set uuid succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  // update to a new valid status
  rv = nrh_set_last_error(NRH_MISSING_LICENSE);

  // write new file (uuid) + same location + new status
  rv = nrh_write_health(location);
  tlib_pass_if_true("health file write succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_exists("./health-ffffffffffffffffffffffffffffffff.yml");

  // update to new valid status
  rv = nrh_set_last_error(NRH_MISSING_APPNAME);

  // update existing file with new status
  rv = nrh_write_health(location);
  tlib_pass_if_true("write_health succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);

  // update new random uuid
  rv = nrh_set_uuid(NULL);
  tlib_pass_if_true("set random uuid succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);

  // verify new random uuid != previous
  rand_uuid = nrh_get_uuid();
  tlib_pass_if_not_null("get uuid succeeds", rand_uuid);
  tlib_pass_if_true(
      "manual uuid successfully replaced by random uuid",
      0 != nr_strcmp("ffffffffffffffffffffffffffffffff", rand_uuid), "rand=%s",
      rand_uuid);

  // update to valid status
  rv = nrh_set_last_error(NRH_CONNECTION_FAILED);

  // write new file (random uuid) + existing location + new status
  rv = nrh_write_health(location);

  // test get_health_filename functionality
  rand_healthfile = nrh_get_health_filename();
  tlib_pass_if_not_null("get health filename succeeds", rand_healthfile);
  tlib_pass_if_exists(rand_healthfile);

  // clean up
  nr_unlink("health-bc21b5891f5e44fc9272caef924611a8.yml");
  nr_unlink("health-ffffffffffffffffffffffffffffffff.yml");
  nr_unlink(rand_healthfile);

  nr_free(location);
  nr_free(rand_uuid);
  nr_free(rand_healthfile);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_health();
}
