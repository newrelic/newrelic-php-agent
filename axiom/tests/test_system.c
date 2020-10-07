/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"

#include "tlib_main.h"

static void test_system_get_hostname(void) {
  char* hostname;

  hostname = nr_system_get_hostname();
  tlib_pass_if_true("hostname not null", 0 != hostname, "hostname=%p",
                    hostname);
  nr_free(hostname);
}

static void test_get_system(void) {
  nr_system_t* sys = nr_system_get_system_information();

  tlib_pass_if_true("sys not null", 0 != sys, "sys=%p", sys);

  if (0 == sys) {
    return;
  }

#if defined(__linux__)
  tlib_pass_if_true("sys->sysname", 0 == nr_strcmp(sys->sysname, "Linux"),
                    "expected sysname=Linux result=%s", sys->sysname);
#elif defined(__APPLE__) && defined(__MACH__)
  tlib_pass_if_true("sys->sysname", 0 == nr_strcmp(sys->sysname, "Darwin"),
                    "expected sysname=Darwin result=%s", sys->sysname);
#elif defined(__sun__) || defined(__sun)
  tlib_pass_if_true("sys->sysname",
                    (0 == nr_strcmp(sys->sysname, "SunOS"))
                        || (0 == nr_strcmp(sys->sysname, "SmartOS")),
                    "expected sysname=SunOS/SmartOS result=%s", sys->sysname);
#elif defined(__FreeBSD__)
  tlib_pass_if_true("sys->sysname", 0 == nr_strcmp(sys->sysname, "FreeBSD"),
                    "expected sysname=FreeBSD result=%s", sys->sysname);
#else
#error Unsupported OS: please add the expected uname to this file.
#endif

  tlib_pass_if_true("sys value not null", 0 != sys->nodename,
                    "sys->nodename=%p", sys->nodename);
  tlib_pass_if_true("sys value not null", 0 != sys->release, "sys->release=%p",
                    sys->release);
  tlib_pass_if_true("sys value not null", 0 != sys->version, "sys->version=%p",
                    sys->version);
  tlib_pass_if_true("sys value not null", 0 != sys->machine, "sys->machine=%p",
                    sys->machine);

  nr_system_destroy(&sys);
  tlib_pass_if_true("sys destroyed", 0 == sys, "sys=%p", sys);
}

static void test_system_destroy_bad_params(void) {
  nr_system_t* sys;

  /* Don't blow up! */
  nr_system_destroy(0);
  sys = 0;
  nr_system_destroy(&sys);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_system_get_hostname();
  test_get_system();
  test_system_destroy_bad_params();
}
